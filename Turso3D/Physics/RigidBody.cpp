// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../IO/ResourceRef.h"
#include "../Math/Matrix3x4.h"
#include "../Renderer/Model.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Scene.h"
#include "JoltUtils.h"
#include "PhysicsWorld.h"
#include "RigidBody.h"

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

static const float MIN_SHAPE_SIZE = 0.001f;

RigidBody::RigidBody() :
    world(nullptr),
    body(nullptr),
    shapeSize(Vector3::ZERO),
    shapeWorldScale(Vector3::ONE),
    prevPosition(Vector3::ZERO),
    prevRotation(Quaternion::IDENTITY),
    mass(0.0f),
    collisionLayer(DEFAULT_COLLISION_LAYER),
    collisionMask(DEFAULT_COLLISION_MASK),
    contactListIndex(-1),
    shapeType(SHAPE_NONE),
    kinematic(false),
    transformUpdateQueued(false),
    kinematicMoveQueued(false),
    interpolated(false)
{
}

RigidBody::~RigidBody()
{
    if (world)
        world->RemoveRigidBody(this);
}

void RigidBody::RegisterObject()
{
    RegisterFactory<RigidBody>();
    RegisterDerivedType<RigidBody, SpatialNode>();
    CopyBaseAttributes<RigidBody, SpatialNode>();
    RegisterAttribute("kinematic", &RigidBody::IsKinematic, &RigidBody::SetKinematic, false);
    RegisterAttribute("mass", &RigidBody::Mass, &RigidBody::SetMass, 0.0f);
    // Reading this attribute sets all shape parameters at once and triggers the Jolt body creation, which is the heavy operation
    RegisterMixedRefAttribute("shape", &RigidBody::ShapeAttr, &RigidBody::SetShapeAttr);
    // These attributes are registered last because they are applied to the existing Jolt body without recreating it
    RegisterAttribute("collisionLayer", &RigidBody::CollisionLayer, &RigidBody::SetCollisionLayer, DEFAULT_COLLISION_LAYER);
    RegisterAttribute("collisionMask", &RigidBody::CollisionMask, &RigidBody::SetCollisionMask, DEFAULT_COLLISION_MASK);
    RegisterMixedRefAttribute("linearVelocity", &RigidBody::LinearVelocity, &RigidBody::SetLinearVelocity, Vector3::ZERO);
    RegisterMixedRefAttribute("angularVelocity", &RigidBody::AngularVelocity, &RigidBody::SetAngularVelocity, Vector3::ZERO);
}

void RigidBody::SetMass(float newMass)
{
    newMass = Max(newMass, 0.0f);
    if (newMass != mass)
    {
        mass = newMass;
        RecreateBody();
    }
}

void RigidBody::SetKinematic(bool enable)
{
    if (enable != kinematic)
    {
        kinematic = enable;
        RecreateBody();
    }
}

void RigidBody::SetBoxShape(const Vector3& size)
{
    SetShape(SHAPE_BOX, size);
}

void RigidBody::SetCapsuleShape(const Vector3& size)
{
    SetShape(SHAPE_CAPSULE, size);
}

void RigidBody::SetCylinderShape(const Vector3& size)
{
    SetShape(SHAPE_CYLINDER, size);
}

void RigidBody::SetSphereShape(float diameter)
{
    SetShape(SHAPE_SPHERE, Vector3(diameter, diameter, diameter));
}

void RigidBody::SetConvexShape(Model* model, const Vector3& modelScale, int lodLevel)
{
    if (model)
        SetShape(SHAPE_CONVEX, modelScale, model, lodLevel);
    else
        RemoveShape();
}

void RigidBody::SetTriangleShape(Model* model, const Vector3& modelScale, int lodLevel)
{
    if (model)
        SetShape(SHAPE_TRIANGLEMESH, modelScale, model, lodLevel);
    else
        RemoveShape();
}

void RigidBody::RemoveShape()
{
    SetShape(SHAPE_NONE, Vector3::ZERO, nullptr);
}

void RigidBody::SetCollisionLayer(unsigned layer)
{
    if (layer != collisionLayer)
    {
        collisionLayer = layer;
        ApplyCollisionFilter();
    }
}

void RigidBody::SetCollisionMask(unsigned mask)
{
    if (mask != collisionMask)
    {
        collisionMask = mask;
        ApplyCollisionFilter();
    }
}

void RigidBody::SetLinearVelocity(const Vector3& linearVelocity)
{
    if (body && !body->IsStatic())
        world->JoltBodyInterface().SetLinearVelocity(body->GetID(), ToJolt(linearVelocity));
}

void RigidBody::SetAngularVelocity(const Vector3& angularVelocity)
{
    if (body && !body->IsStatic())
        world->JoltBodyInterface().SetAngularVelocity(body->GetID(), ToJolt(angularVelocity));
}

void RigidBody::AddForce(const Vector3& force)
{
    if (body && body->IsDynamic())
        world->JoltBodyInterface().AddForce(body->GetID(), ToJolt(force), JPH::EActivation::Activate);
}

void RigidBody::AddForce(const Vector3& force, const Vector3& offset)
{
    if (body && body->IsDynamic())
        world->JoltBodyInterface().AddForce(body->GetID(), ToJolt(force), body->GetPosition() + ToJolt(offset), JPH::EActivation::Activate);
}

void RigidBody::AddTorque(const Vector3& torque)
{
    if (body && body->IsDynamic())
        world->JoltBodyInterface().AddTorque(body->GetID(), ToJolt(torque), JPH::EActivation::Activate);
}

void RigidBody::AddImpulse(const Vector3& impulse)
{
    if (body && body->IsDynamic())
        world->JoltBodyInterface().AddImpulse(body->GetID(), ToJolt(impulse));
}

void RigidBody::Activate()
{
    if (body && !body->IsStatic())
        world->JoltBodyInterface().ActivateBody(body->GetID());
}

Vector3 RigidBody::LinearVelocity() const
{
    return body ? ToVector3(body->GetLinearVelocity()) : Vector3::ZERO;
}

Vector3 RigidBody::AngularVelocity() const
{
    return body ? ToVector3(body->GetAngularVelocity()) : Vector3::ZERO;
}

bool RigidBody::IsActive() const
{
    return body ? body->IsActive() : false;
}

Model* RigidBody::ShapeModel() const
{
    return shapeModel.Get();
}

void RigidBody::ApplyTransform(const Vector3& newPosition, const Quaternion& newRotation)
{
    SpatialNode* parentNode = SpatialParent();
    if (parentNode)
    {
        position = parentNode->WorldTransform().Inverse() * newPosition;
        rotation = parentNode->WorldRotation().Inverse() * newRotation;
    }
    else
    {
        position = newPosition;
        rotation = newRotation;
    }

    // Dirty the world transform and child world transforms, but do not signal change to simulation
    SpatialNode::OnTransformChanged();
}

void RigidBody::OnSceneSet(Scene* newScene, Scene* /*oldScene*/)
{
    if (world)
        world->RemoveRigidBody(this);

    if (newScene)
    {
        // Physics world must be attached to the scene root as a child
        PhysicsWorld* newWorld = newScene->FindChild<PhysicsWorld>();
        if (newWorld)
            newWorld->AddRigidBody(this);
    }
}

void RigidBody::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();

    // Defer applying the transform to the Jolt body, so that several transform changes per frame are cheap.
    // Also, if a hierarchical update causes this to be called during PhysicsWorld::ApplyTransforms(),
    // do not signal back to the world
    if (body && !world->inApplyTransforms)
        world->QueueTransformUpdate(this);
}

void RigidBody::OnEnabledChanged(bool newEnabled)
{
    if (newEnabled)
        CreateBody();
    else
        ReleaseBody();
}

void RigidBody::SetShape(CollisionShapeType type, const Vector3& size, Model* model, int lodLevel)
{
    shapeType = type;
    shapeSize = size;
    shapeModel = model;
    shapeModelLod = Clamp(lodLevel, 0, model ? (int)model->NumLodLevels(0) - 1 : 0);
    RecreateBody();
}

void RigidBody::CreateBody(const Vector3& linearVelocity, const Vector3& angularVelocity)
{
    if (body || !world || shapeType == SHAPE_NONE || !IsEnabled())
        return;

    Vector3 worldScale = WorldScale();
    Vector3 absScale = worldScale.Abs();
    JPH::Shape::ShapeResult shapeResult;

    switch (shapeType)
    {
    case SHAPE_BOX:
        {
            Vector3 halfExtents = 0.5f * shapeSize * absScale;
            halfExtents = Vector3(Max(halfExtents.x, MIN_SHAPE_SIZE), Max(halfExtents.y, MIN_SHAPE_SIZE), Max(halfExtents.z, MIN_SHAPE_SIZE));
            float convexRadius = Min(JPH::cDefaultConvexRadius, Min(halfExtents.x, Min(halfExtents.y, halfExtents.z)));
            shapeResult = JPH::BoxShapeSettings(ToJolt(halfExtents), convexRadius).Create();
        }
        break;

    case SHAPE_SPHERE:
        {
            float radius = Max(0.5f * shapeSize.x * Max(absScale.x, Max(absScale.y, absScale.z)), MIN_SHAPE_SIZE);
            shapeResult = JPH::SphereShapeSettings(radius).Create();
        }
        break;

    case SHAPE_CAPSULE:
        {
            float radius = Max(0.5f * shapeSize.x * Max(absScale.x, absScale.z), MIN_SHAPE_SIZE);
            float halfHeight = 0.5f * shapeSize.y * absScale.y - radius;
            // If the capsule has no cylindrical part, it is a sphere
            if (halfHeight >= MIN_SHAPE_SIZE)
                shapeResult = JPH::CapsuleShapeSettings(halfHeight, radius).Create();
            else
                shapeResult = JPH::SphereShapeSettings(radius).Create();
        }
        break;

    case SHAPE_CYLINDER:
        {
            float radius = Max(0.5f * shapeSize.x * Max(absScale.x, absScale.z), MIN_SHAPE_SIZE);
            float halfHeight = Max(0.5f * shapeSize.y * absScale.y, MIN_SHAPE_SIZE);
            float convexRadius = Min(JPH::cDefaultConvexRadius, Min(radius, halfHeight));
            shapeResult = JPH::CylinderShapeSettings(halfHeight, radius, convexRadius).Create();
        }
        break;

    case SHAPE_CONVEX:
    case SHAPE_TRIANGLEMESH:
        {
            JPH::Shape* modelShape = world->ModelShape(shapeModel, shapeModelLod, shapeType == SHAPE_CONVEX);
            if (!modelShape)
                return;

            // Model shapes are cached in unscaled form, so apply scaling with a decorator shape when needed
            Vector3 totalScale = shapeSize * worldScale;
            if (totalScale.Equals(Vector3::ONE))
                shapeResult.Set(modelShape);
            else
                shapeResult.Set(new JPH::ScaledShape(modelShape, ToJolt(totalScale)));
        }
        break;

    default:
        return;
    }

    if (shapeResult.HasError())
    {
        LOGERROR("Failed to create collision shape for rigid body: " + std::string(shapeResult.GetError().c_str()));
        return;
    }

    JPH::EMotionType motionType = kinematic ? JPH::EMotionType::Kinematic : (mass > 0.0f ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static);
    JPH::ObjectLayer objectLayer = motionType == JPH::EMotionType::Static ? PhysicsLayers::NON_MOVING : PhysicsLayers::MOVING;

    if (motionType == JPH::EMotionType::Dynamic && shapeType == SHAPE_TRIANGLEMESH)
        LOGWARNING("Triangle mesh shapes should only be used for static or kinematic rigid bodies");

    Vector3 bodyPosition = WorldPosition();
    Quaternion bodyRotation = WorldRotation().Normalized();

    JPH::BodyCreationSettings settings(shapeResult.Get(), ToJoltR(bodyPosition), ToJolt(bodyRotation), motionType, objectLayer);
    settings.mUserData = (JPH::uint64)(uintptr_t)this;
    settings.mCollisionGroup = JPH::CollisionGroup(PhysicsWorld::CollisionFilter(), collisionLayer, collisionMask);
    if (motionType == JPH::EMotionType::Dynamic)
    {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = mass;
        settings.mLinearVelocity = ToJolt(linearVelocity);
        settings.mAngularVelocity = ToJolt(angularVelocity);
    }

    JPH::BodyInterface& bodyInterface = world->JoltBodyInterface();
    body = bodyInterface.CreateBody(settings);
    if (!body)
    {
        LOGERROR("Failed to create rigid body, maximum amount of bodies reached");
        return;
    }

    bodyInterface.AddBody(body->GetID(), motionType == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
    ++world->numBodiesAddedSinceOptimize;

    shapeWorldScale = worldScale;
    prevPosition = bodyPosition;
    prevRotation = bodyRotation;
}

void RigidBody::ReleaseBody()
{
    if (!body)
        return;

    world->RemoveFromQueues(this);
    world->RemoveContacts(this);

    JPH::BodyInterface& bodyInterface = world->JoltBodyInterface();
    JPH::BodyID id = body->GetID();
    bodyInterface.RemoveBody(id);
    bodyInterface.DestroyBody(id);
    body = nullptr;
}

void RigidBody::RecreateBody()
{
    Vector3 linearVelocity = LinearVelocity();
    Vector3 angularVelocity = AngularVelocity();

    ReleaseBody();
    CreateBody(linearVelocity, angularVelocity);
}

void RigidBody::ApplyNodeTransform()
{
    if (!body)
        return;

    // Scale change requires recreating the shape
    if (!WorldScale().Equals(shapeWorldScale, 0.0001f))
    {
        RecreateBody();
        return;
    }

    // Kinematic bodies are moved during the simulation steps to give them correct velocities
    if (body->IsKinematic())
    {
        world->QueueKinematicMove(this);
        return;
    }

    Vector3 bodyPosition = WorldPosition();
    Quaternion bodyRotation = WorldRotation().Normalized();

    world->JoltBodyInterface().SetPositionAndRotation(body->GetID(), ToJoltR(bodyPosition), ToJolt(bodyRotation),
        body->IsStatic() ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);

    // The body was teleported, so do not interpolate from the old position
    prevPosition = bodyPosition;
    prevRotation = bodyRotation;
}

void RigidBody::ApplyCollisionFilter()
{
    if (!body)
        return;

    body->SetCollisionGroup(JPH::CollisionGroup(PhysicsWorld::CollisionFilter(), collisionLayer, collisionMask));
    // Wake up so that the new filtering takes effect, for example to fall through a body that is no longer collided with
    if (!body->IsStatic())
        world->JoltBodyInterface().ActivateBody(body->GetID());
}

void RigidBody::SetShapeAttr(const JSONValue& value)
{
    if (value.Size() < 6)
        return;

    ResourceCache* cache = Subsystem<ResourceCache>();
    SetShape((CollisionShapeType)value[0].GetNumber(), Vector3((float)value[1].GetNumber(), (float)value[2].GetNumber(), (float)value[3].GetNumber()),
        cache->LoadResource<Model>(ResourceRef(value[4].GetString()).name), value[5].GetNumber());
}

JSONValue RigidBody::ShapeAttr() const
{
    JSONValue shape;
    shape.Push((int)shapeType);
    shape.Push(shapeSize.x);
    shape.Push(shapeSize.y);
    shape.Push(shapeSize.z);
    shape.Push(shapeModel ? MakeResourceRef(shapeModel, Model::TypeStatic()).ToString() : JSONValue::emptyString);
    shape.Push(shapeModelLod);

    return shape;
}
