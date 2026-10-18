// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Math/Ray.h"
#include "../Math/Sphere.h"
#include "../Renderer/DebugRenderer.h"
#include "../Renderer/GeometryNode.h"
#include "../Renderer/Model.h"
#include "../Scene/Scene.h"
#include "JoltUtils.h"
#include "PhysicsWorld.h"
#include "RigidBody.h"
#include "WorkQueueJobSystem.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <tracy/Tracy.hpp>

static const unsigned MAX_BODIES = 65536;
static const unsigned NUM_BODY_MUTEXES = 0; // Autodetect
static const unsigned MAX_BODY_PAIRS = 65536;
static const unsigned MAX_CONTACT_CONSTRAINTS = 16384;
static const unsigned TEMP_ALLOCATOR_SIZE = 16 * 1024 * 1024;
static const unsigned OPTIMIZE_BROADPHASE_THRESHOLD = 256;
static const float MAX_RAY_LENGTH_MARGIN = 1.0f;

static const Vector3 DEFAULT_GRAVITY(0.0f, -9.81f, 0.0f);
static const int DEFAULT_MAX_STEPS = 5;
static const float DEFAULT_TIMESTEP = 1.0f / 60.0f;

/// Task for applying simulation transforms back to the rigid body scene nodes.
struct ApplyTransformsTask : public Task
{
    ApplyTransformsTask(PhysicsWorld* world_) :
        world(world_)
    {
    }

    void Complete(unsigned /*threadIndex*/)
    {
        ZoneScopedN("ApplyTransforms");

        JPH::BodyInterface& bodyInterface = world->JoltBodyInterface();
        float t = Clamp(world->timeAccumulator / world->timeStep, 0.0f, 1.0f);

        for (size_t i = start; i < end; ++i)
        {
            RigidBody* rigidBody = world->interpolatedBodies[i];
            JPH::BodyID id = rigidBody->body->GetID();

            JPH::RVec3 joltPosition;
            JPH::Quat joltRotation;
            bodyInterface.GetPositionAndRotation(id, joltPosition, joltRotation);
            Vector3 position = ToVector3(joltPosition);
            Quaternion rotation = ToQuaternion(joltRotation);

            bool active = rigidBody->body->IsActive();
            // If the body went to sleep, snap to the final position and stop interpolating it
            if (!active)
            {
                rigidBody->prevPosition = position;
                rigidBody->prevRotation = rotation;
            }

            rigidBody->ApplyTransform(rigidBody->prevPosition.Lerp(position, t), rigidBody->prevRotation.Slerp(rotation, t));
        }
    }

    PhysicsWorld* world;
    size_t start;
    size_t end;
};

/// Cached Jolt shapes created from models.
struct ModelShapeCache
{
    /// Cache entry.
    struct Entry
    {
        /// Weak pointer to model to detect its possible destruction.
        WeakPtr<Model> model;
        /// Jolt shape.
        JPH::Ref<JPH::Shape> shape;
    };

    /// Cached shapes by model and convex flag.
    std::map<std::pair<Model*, bool>, Entry> entries;
};

#ifdef JPH_DEBUG_RENDERER
/// Jolt debug renderer that outputs lines to the Turso3D debug renderer.
class JoltDebugRenderer : public JPH::DebugRendererSimple
{
public:
    /// Draw a line.
    void DrawLine(JPH::RVec3Arg from, JPH::RVec3Arg to, JPH::ColorArg color) override
    {
        if (debug)
            debug->AddLine(ToVector3(from), ToVector3(to), color.GetUInt32());
    }

    /// Draw text. Not supported.
    void DrawText3D(JPH::RVec3Arg, const std::string_view&, JPH::ColorArg, float) override
    {
    }

    /// Current Turso3D debug renderer.
    ::DebugRenderer* debug = nullptr;
};

/// Body draw filter that culls bodies outside the debug renderer's view frustum.
class JoltBodyDrawFilter : public JPH::BodyDrawFilter
{
public:
    /// Construct.
    JoltBodyDrawFilter(::DebugRenderer* debug_) :
        debug(debug_)
    {
    }

    /// Return whether should draw a body.
    bool ShouldDraw(const JPH::Body& body) const override
    {
        return debug->IsInside(ToBoundingBox(body.GetWorldSpaceBounds()));
    }

    /// Turso3D debug renderer.
    ::DebugRenderer* debug;
};

static JoltDebugRenderer* joltDebugRenderer = nullptr;
#endif

static inline unsigned ReadIndex(const unsigned char* indexData, size_t indexSize, size_t index)
{
    return indexSize == sizeof(unsigned) ? reinterpret_cast<const unsigned*>(indexData)[index] : reinterpret_cast<const unsigned short*>(indexData)[index];
}

PhysicsWorld::PhysicsWorld() :
    gravity(DEFAULT_GRAVITY),
    timeStep(DEFAULT_TIMESTEP),
    maxSteps(DEFAULT_MAX_STEPS),
    timeAccumulator(0.0f),
    numBodiesAddedSinceOptimize(0),
    modelShapes(new ModelShapeCache()),
    inApplyTransforms(false)
{
    jobSystem = new WorkQueueJobSystem();
    tempAllocator = new JPH::TempAllocatorImplWithMallocFallback(TEMP_ALLOCATOR_SIZE);

    broadPhaseLayerInterface = new JPH::BroadPhaseLayerInterfaceTable(PhysicsLayers::NUM_LAYERS, PhysicsBroadPhaseLayers::NUM_LAYERS);
    broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(PhysicsLayers::NON_MOVING, PhysicsBroadPhaseLayers::NON_MOVING);
    broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(PhysicsLayers::MOVING, PhysicsBroadPhaseLayers::MOVING);

    objectLayerPairFilter = new JPH::ObjectLayerPairFilterTable(PhysicsLayers::NUM_LAYERS);
    objectLayerPairFilter->EnableCollision(PhysicsLayers::MOVING, PhysicsLayers::MOVING);
    objectLayerPairFilter->EnableCollision(PhysicsLayers::MOVING, PhysicsLayers::NON_MOVING);

    objectVsBroadPhaseLayerFilter = new JPH::ObjectVsBroadPhaseLayerFilterTable(*broadPhaseLayerInterface, PhysicsBroadPhaseLayers::NUM_LAYERS, *objectLayerPairFilter, PhysicsLayers::NUM_LAYERS);

    physicsSystem = new JPH::PhysicsSystem();
    physicsSystem->Init(MAX_BODIES, NUM_BODY_MUTEXES, MAX_BODY_PAIRS, MAX_CONTACT_CONSTRAINTS, *broadPhaseLayerInterface, *objectVsBroadPhaseLayerFilter, *objectLayerPairFilter);
    physicsSystem->SetGravity(ToJolt(gravity));

    workQueue = Subsystem<WorkQueue>();
}

PhysicsWorld::~PhysicsWorld()
{
    RemoveAllRigidBodies();

    // Release cached shapes before the physics system and Jolt global state
    modelShapes.Reset();
}

void PhysicsWorld::RegisterObject()
{
    // Register physics world allocator with small initial capacity with the assumption that we don't create many of them
    RegisterFactory<PhysicsWorld>(1);
    RegisterDerivedType<PhysicsWorld, Node>();
    CopyBaseAttributes<PhysicsWorld, Node>();
    RegisterRefAttribute("gravity", &PhysicsWorld::Gravity, &PhysicsWorld::SetGravity, DEFAULT_GRAVITY);
    RegisterAttribute("timestep", &PhysicsWorld::TimeStep, &PhysicsWorld::SetTimeStep, DEFAULT_TIMESTEP);
    RegisterAttribute("maxSteps", &PhysicsWorld::MaxSteps, &PhysicsWorld::SetMaxSteps, DEFAULT_MAX_STEPS);
}

void PhysicsWorld::Update(float deltaTime)
{
    ZoneScoped;

    ProcessTransformUpdates();

    timeAccumulator += deltaTime;
    // Clamp the accumulated time to prevent a "spiral of death" if the simulation can not keep up
    if (timeAccumulator > timeStep * maxSteps)
        timeAccumulator = timeStep * maxSteps;

    bool stepped = false;

    while (timeAccumulator >= timeStep)
    {
        StorePreviousTransforms(true);
        MoveKinematicBodies();

        if (numBodiesAddedSinceOptimize >= OPTIMIZE_BROADPHASE_THRESHOLD)
        {
            physicsSystem->OptimizeBroadPhase();
            numBodiesAddedSinceOptimize = 0;
        }

        JPH::EPhysicsUpdateError error = physicsSystem->Update(timeStep, 1, tempAllocator, jobSystem);
        if (error != JPH::EPhysicsUpdateError::None)
            LOGWARNINGF("Physics update error %u", (unsigned)error);

        timeAccumulator -= timeStep;
        stepped = true;
    }

    // Bodies may have been woken up during the step, mark them for interpolation without changing the previous transforms
    if (stepped)
        StorePreviousTransforms(false);
}

void PhysicsWorld::ApplyTransforms()
{
    ZoneScoped;

    inApplyTransforms = true;

    size_t taskIdx = 0;
    size_t bodiesPerTask = Max((size_t)16, interpolatedBodies.size() / workQueue->NumThreads());

    for (size_t start = 0; start < interpolatedBodies.size(); start += bodiesPerTask)
    {
        size_t end = start + bodiesPerTask;
        if (end > interpolatedBodies.size())
            end = interpolatedBodies.size();
        if (applyTransformsTasks.size() <= taskIdx)
            applyTransformsTasks.push_back(new ApplyTransformsTask(this));

        applyTransformsTasks[taskIdx]->start = start;
        applyTransformsTasks[taskIdx]->end = end;
        ++taskIdx;
    }
    
    if (taskIdx > 0)
    {
        workQueue->QueueTasks(taskIdx, reinterpret_cast<Task**>(&applyTransformsTasks[0]));
        workQueue->Complete();
    }

    inApplyTransforms = false;

    // Single-threaded poststep: remove bodies which no longer need to interpolate
    for (size_t i = 0; i < interpolatedBodies.size();)
    {
        RigidBody* rigidBody = interpolatedBodies[i];
        bool active = rigidBody->body->IsActive();
        if (active)
            ++i;
        else
        {
            rigidBody->interpolated = false;
            interpolatedBodies[i] = interpolatedBodies.back();
            interpolatedBodies.pop_back();
        }
    }
}

void PhysicsWorld::SetGravity(const Vector3& gravity_)
{
    gravity = gravity_;
    physicsSystem->SetGravity(ToJolt(gravity));
}

void PhysicsWorld::SetTimeStep(float timeStep_)
{
    timeStep = Max(timeStep_, M_EPSILON);
}

void PhysicsWorld::SetMaxSteps(int maxSteps_)
{
    maxSteps = Max(maxSteps_, 1);
}

void PhysicsWorld::Raycast(std::vector<PhysicsRaycastResult>& result, const Ray& ray, float maxDistance) const
{
    ZoneScoped;

    result.clear();

    const_cast<PhysicsWorld*>(this)->ProcessTransformUpdates();

    // Limit the ray length to the extent of the physics world to avoid overflow with infinite distance
    JPH::AABox bounds = physicsSystem->GetBroadPhaseQuery().GetBounds();
    if (!bounds.IsValid())
        return;

    JPH::Vec3 origin = ToJolt(ray.origin);
    float worldExtent = JPH::Vec3::sMax((origin - bounds.mMin).Abs(), (origin - bounds.mMax).Abs()).Length() + MAX_RAY_LENGTH_MARGIN;
    float length = Min(maxDistance, worldExtent);
    if (length <= 0.0f)
        return;

    JPH::RRayCast joltRay(ToJoltR(ray.origin), ToJolt(ray.direction) * length);
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
    physicsSystem->GetNarrowPhaseQueryNoLock().CastRay(joltRay, JPH::RayCastSettings(), collector);
    collector.Sort();

    const JPH::BodyLockInterfaceNoLock& lockInterface = physicsSystem->GetBodyLockInterfaceNoLock();

    for (auto it = collector.mHits.begin(); it != collector.mHits.end(); ++it)
    {
        JPH::BodyLockRead lock(lockInterface, it->mBodyID);
        if (!lock.Succeeded())
            continue;

        const JPH::Body& hitBody = lock.GetBody();
        JPH::RVec3 hitPoint = joltRay.GetPointOnRay(it->mFraction);

        PhysicsRaycastResult res;
        res.rigidBody = reinterpret_cast<RigidBody*>((uintptr_t)hitBody.GetUserData());
        res.position = ToVector3(hitPoint);
        res.normal = ToVector3(hitBody.GetWorldSpaceSurfaceNormal(it->mSubShapeID2, hitPoint));
        res.distance = it->mFraction * length;
        result.push_back(res);
    }
}

PhysicsRaycastResult PhysicsWorld::RaycastSingle(const Ray& ray, float maxDistance) const
{
    ZoneScoped;

    PhysicsRaycastResult res;
    res.rigidBody = nullptr;
    res.position = Vector3::ZERO;
    res.normal = Vector3::ZERO;
    res.distance = M_MAX_FLOAT;

    const_cast<PhysicsWorld*>(this)->ProcessTransformUpdates();

    JPH::AABox bounds = physicsSystem->GetBroadPhaseQuery().GetBounds();
    if (!bounds.IsValid())
        return res;

    JPH::Vec3 origin = ToJolt(ray.origin);
    float worldExtent = JPH::Vec3::sMax((origin - bounds.mMin).Abs(), (origin - bounds.mMax).Abs()).Length() + MAX_RAY_LENGTH_MARGIN;
    float length = Min(maxDistance, worldExtent);
    if (length <= 0.0f)
        return res;

    JPH::RRayCast joltRay(ToJoltR(ray.origin), ToJolt(ray.direction) * length);
    JPH::RayCastResult hit;
    if (!physicsSystem->GetNarrowPhaseQueryNoLock().CastRay(joltRay, hit))
        return res;

    JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterfaceNoLock(), hit.mBodyID);
    if (!lock.Succeeded())
        return res;

    const JPH::Body& hitBody = lock.GetBody();
    JPH::RVec3 hitPoint = joltRay.GetPointOnRay(hit.mFraction);

    res.rigidBody = reinterpret_cast<RigidBody*>((uintptr_t)hitBody.GetUserData());
    res.position = ToVector3(hitPoint);
    res.normal = ToVector3(hitBody.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPoint));
    res.distance = hit.mFraction * length;
    return res;
}

void PhysicsWorld::SphereQuery(std::vector<RigidBody*>& result, const Sphere& sphere)
{
    ZoneScoped;

    result.clear();

    if (sphere.radius <= 0.0f)
        return;

    ProcessTransformUpdates();

    JPH::SphereShape sphereShape(sphere.radius);
    sphereShape.SetEmbedded();

    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
    physicsSystem->GetNarrowPhaseQueryNoLock().CollideShape(&sphereShape, JPH::Vec3::sOne(), JPH::RMat44::sTranslation(ToJoltR(sphere.center)),
        JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector);

    // There may be several hits per body, so sort and remove duplicates
    std::vector<JPH::BodyID> hitBodies;
    for (auto it = collector.mHits.begin(); it != collector.mHits.end(); ++it)
        hitBodies.push_back(it->mBodyID2);
    std::sort(hitBodies.begin(), hitBodies.end());
    hitBodies.erase(std::unique(hitBodies.begin(), hitBodies.end()), hitBodies.end());

    JPH::BodyInterface& bodyInterface = JoltBodyInterface();
    for (auto it = hitBodies.begin(); it != hitBodies.end(); ++it)
    {
        RigidBody* rigidBody = reinterpret_cast<RigidBody*>((uintptr_t)bodyInterface.GetUserData(*it));
        if (rigidBody)
            result.push_back(rigidBody);
    }
}

void PhysicsWorld::OnRenderDebug(DebugRenderer* debug)
{
#ifdef JPH_DEBUG_RENDERER
    ZoneScoped;

    if (!debug)
        return;

    if (!joltDebugRenderer)
        joltDebugRenderer = new JoltDebugRenderer();

    joltDebugRenderer->debug = debug;

    JPH::BodyManager::DrawSettings settings;
    settings.mDrawShape = true;
    settings.mDrawShapeWireframe = true;
    JoltBodyDrawFilter filter(debug);

    physicsSystem->DrawBodies(settings, joltDebugRenderer, &filter);

    joltDebugRenderer->debug = nullptr;
#else
    (void)debug;
#endif
}

void PhysicsWorld::AddRigidBody(RigidBody* rigidBody)
{
    if (!rigidBody || rigidBody->world == this)
        return;

    rigidBodies.push_back(rigidBody);
    rigidBody->world = this;
    rigidBody->CreateBody();
}

void PhysicsWorld::RemoveRigidBody(RigidBody* rigidBody)
{
    if (!rigidBody || rigidBody->world != this)
        return;

    rigidBody->ReleaseBody();
    rigidBody->world = nullptr;

    auto it = std::find(rigidBodies.begin(), rigidBodies.end(), rigidBody);
    if (it != rigidBodies.end())
    {
        *it = rigidBodies.back();
        rigidBodies.pop_back();
    }
}

void PhysicsWorld::RemoveAllRigidBodies()
{
    for (auto it = rigidBodies.begin(); it != rigidBodies.end(); ++it)
    {
        RigidBody* rigidBody = *it;
        rigidBody->ReleaseBody();
        rigidBody->world = nullptr;
    }

    rigidBodies.clear();
    transformUpdates.clear();
    kinematicMoves.clear();
    interpolatedBodies.clear();
}

void PhysicsWorld::QueueTransformUpdate(RigidBody* rigidBody)
{
    if (!rigidBody->transformUpdateQueued)
    {
        rigidBody->transformUpdateQueued = true;
        transformUpdates.push_back(rigidBody);
    }
}

void PhysicsWorld::QueueKinematicMove(RigidBody* rigidBody)
{
    if (!rigidBody->kinematicMoveQueued)
    {
        rigidBody->kinematicMoveQueued = true;
        kinematicMoves.push_back(rigidBody);
    }
}

void PhysicsWorld::RemoveFromQueues(RigidBody* rigidBody)
{
    if (rigidBody->transformUpdateQueued)
    {
        transformUpdates.erase(std::find(transformUpdates.begin(), transformUpdates.end(), rigidBody));
        rigidBody->transformUpdateQueued = false;
    }
    if (rigidBody->kinematicMoveQueued)
    {
        kinematicMoves.erase(std::find(kinematicMoves.begin(), kinematicMoves.end(), rigidBody));
        rigidBody->kinematicMoveQueued = false;
    }
    if (rigidBody->interpolated)
    {
        interpolatedBodies.erase(std::find(interpolatedBodies.begin(), interpolatedBodies.end(), rigidBody));
        rigidBody->interpolated = false;
    }
}

void PhysicsWorld::ProcessTransformUpdates()
{
    if (transformUpdates.empty())
        return;

    // Applying the transform may recreate the body, so swap the queue first
    std::vector<RigidBody*> updates;
    updates.swap(transformUpdates);

    for (auto it = updates.begin(); it != updates.end(); ++it)
    {
        RigidBody* rigidBody = *it;
        rigidBody->transformUpdateQueued = false;
        rigidBody->ApplyNodeTransform();
    }
}

void PhysicsWorld::StorePreviousTransforms(bool storeTransforms)
{
    JPH::BodyIDVector activeBodies;
    physicsSystem->GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies);

    const JPH::BodyLockInterfaceNoLock& lockInterface = physicsSystem->GetBodyLockInterfaceNoLock();

    for (auto it = activeBodies.begin(); it != activeBodies.end(); ++it)
    {
        JPH::BodyLockRead lock(lockInterface, *it);
        if (!lock.Succeeded())
            continue;

        const JPH::Body& activeBody = lock.GetBody();
        if (!activeBody.IsDynamic())
            continue;

        RigidBody* rigidBody = reinterpret_cast<RigidBody*>((uintptr_t)activeBody.GetUserData());
        if (!rigidBody)
            continue;

        if (storeTransforms)
        {
            rigidBody->prevPosition = ToVector3(activeBody.GetPosition());
            rigidBody->prevRotation = ToQuaternion(activeBody.GetRotation());
        }

        if (!rigidBody->interpolated)
        {
            rigidBody->interpolated = true;
            interpolatedBodies.push_back(rigidBody);
        }
    }
}

void PhysicsWorld::MoveKinematicBodies()
{
    JPH::BodyInterface& bodyInterface = JoltBodyInterface();

    for (size_t i = 0; i < kinematicMoves.size();)
    {
        RigidBody* rigidBody = kinematicMoves[i];
        JPH::BodyID id = rigidBody->body->GetID();
        Vector3 position = rigidBody->WorldPosition();
        Quaternion rotation = rigidBody->WorldRotation();

        bodyInterface.MoveKinematic(id, ToJoltR(position), ToJolt(rotation), timeStep);

        // MoveKinematic sets the velocity required to reach the target during the step. Once it results in zero velocity the target has been reached
        // and the body no longer needs to be moved
        if (rigidBody->body->GetLinearVelocity().IsNearZero() && rigidBody->body->GetAngularVelocity().IsNearZero())
        {
            rigidBody->kinematicMoveQueued = false;
            kinematicMoves[i] = kinematicMoves.back();
            kinematicMoves.pop_back();
        }
        else
            ++i;
    }
}

JPH::Shape* PhysicsWorld::ModelShape(Model* model, bool convex)
{
    if (!model)
        return nullptr;

    std::pair<Model*, bool> key(model, convex);
    auto it = modelShapes->entries.find(key);
    if (it != modelShapes->entries.end())
    {
        if (it->second.model.Get() == model)
            return it->second.shape;
        // Stale entry from a destroyed model at the same address
        modelShapes->entries.erase(it);
    }

    ZoneScoped;

    JPH::Shape::ShapeResult shapeResult;

    if (convex)
    {
        JPH::Array<JPH::Vec3> points;

        for (size_t i = 0; i < model->NumGeometries(); ++i)
        {
            const std::vector<SharedPtr<Geometry> >& lodGeometries = model->LodGeometries(i);
            if (lodGeometries.empty())
                continue;

            Geometry* geom = lodGeometries.back();
            if (!geom || !geom->cpuPositionData || !geom->cpuIndexData)
                continue;

            // Use each referenced vertex only once
            std::vector<unsigned> indices;
            indices.reserve(geom->drawCount);
            for (size_t j = 0; j < geom->drawCount; ++j)
                indices.push_back(ReadIndex(geom->cpuIndexData.Get(), geom->cpuIndexSize, geom->cpuDrawStart + j));
            std::sort(indices.begin(), indices.end());
            indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

            for (auto iIt = indices.begin(); iIt != indices.end(); ++iIt)
                points.push_back(ToJolt(geom->cpuPositionData[*iIt]));
        }

        if (points.empty())
        {
            LOGERROR("Model " + model->Name() + " has no CPU-side geometry data for convex shape");
            return nullptr;
        }

        JPH::ConvexHullShapeSettings settings(points);
        shapeResult = settings.Create();
    }
    else
    {
        JPH::VertexList vertices;
        JPH::IndexedTriangleList triangles;

        for (size_t i = 0; i < model->NumGeometries(); ++i)
        {
            const std::vector<SharedPtr<Geometry> >& lodGeometries = model->LodGeometries(i);
            if (lodGeometries.empty())
                continue;

            Geometry* geom = lodGeometries.back();
            if (!geom || !geom->cpuPositionData || !geom->cpuIndexData || geom->drawCount < 3)
                continue;

            // Copy only the vertex range referenced by this geometry
            unsigned minIndex = M_MAX_UNSIGNED;
            unsigned maxIndex = 0;
            for (size_t j = 0; j < geom->drawCount; ++j)
            {
                unsigned index = ReadIndex(geom->cpuIndexData.Get(), geom->cpuIndexSize, geom->cpuDrawStart + j);
                if (index < minIndex)
                    minIndex = index;
                if (index > maxIndex)
                    maxIndex = index;
            }

            unsigned vertexStart = (unsigned)vertices.size();
            for (unsigned j = minIndex; j <= maxIndex; ++j)
            {
                const Vector3& position = geom->cpuPositionData[j];
                vertices.push_back(JPH::Float3(position.x, position.y, position.z));
            }

            for (size_t j = 0; j + 2 < geom->drawCount; j += 3)
            {
                unsigned i1 = ReadIndex(geom->cpuIndexData.Get(), geom->cpuIndexSize, geom->cpuDrawStart + j) - minIndex + vertexStart;
                unsigned i2 = ReadIndex(geom->cpuIndexData.Get(), geom->cpuIndexSize, geom->cpuDrawStart + j + 1) - minIndex + vertexStart;
                unsigned i3 = ReadIndex(geom->cpuIndexData.Get(), geom->cpuIndexSize, geom->cpuDrawStart + j + 2) - minIndex + vertexStart;
                // Store the submesh index as triangle user data for possible later use
                triangles.push_back(JPH::IndexedTriangle(i1, i2, i3, 0, (JPH::uint)i));
            }
        }

        if (triangles.empty())
        {
            LOGERROR("Model " + model->Name() + " has no CPU-side geometry data for triangle mesh shape");
            return nullptr;
        }

        JPH::MeshShapeSettings settings(std::move(vertices), std::move(triangles));
        shapeResult = settings.Create();
    }

    if (shapeResult.HasError())
    {
        LOGERRORF("Failed to create %s shape from model %s: %s", convex ? "convex" : "triangle mesh", model->Name().c_str(), shapeResult.GetError().c_str());
        return nullptr;
    }

    ModelShapeCache::Entry& entry = modelShapes->entries[key];
    entry.model = model;
    entry.shape = shapeResult.Get();
    return entry.shape;
}

JPH::BodyInterface& PhysicsWorld::JoltBodyInterface() const
{
    return physicsSystem->GetBodyInterfaceNoLock();
}

static void JoltTrace(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);

    LOGINFO(std::string("Jolt: ") + buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertFailed(const char* expression, const char* message, const char* file, JPH::uint line)
{
    LOGERRORF("Jolt assert failed: %s:%u: (%s) %s", file, line, expression, message ? message : "");
    // Break into the debugger
    return true;
}
#endif

void RegisterPhysicsLibrary()
{
    static bool registered = false;
    if (registered)
        return;

    RigidBody::RegisterObject();
    PhysicsWorld::RegisterObject();

    // Register Jolt globals
    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTrace;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailed;)
        JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    registered = true;
}
