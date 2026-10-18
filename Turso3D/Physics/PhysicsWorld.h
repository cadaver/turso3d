// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Scene/Node.h"

class DebugRenderer;
class JoltJobSystem;
class PhysicsContactListener;
class Model;
class Ray;
class RigidBody;
class Sphere;
class WorkQueue;
struct ModelShapeCache;
struct ApplyTransformsTask;

namespace JPH
{
    class BodyInterface;
    class BroadPhaseLayerInterfaceTable;
    class GroupFilter;
    class ObjectLayerPairFilterTable;
    class ObjectVsBroadPhaseLayerFilterTable;
    class PhysicsSystem;
    class Shape;
    class TempAllocator;
}

/// Physics raycast result.
struct PhysicsRaycastResult
{
    /// Hit rigidbody node, or null if no hit.
    RigidBody* rigidBody;
    /// World-space hit point.
    Vector3 position;
    /// World-space hit normal.
    Vector3 normal;
    /// Hit distance along the ray.
    float distance;
};

/// Physics simulation world. Should be created as a child of the scene root.
class PhysicsWorld : public Node
{
    OBJECT(PhysicsWorld);

    friend class RigidBody;
    friend struct ApplyTransformsTask;

public:
    /// Construct. The WorkQueue subsystem must have been initialized, as it will be used during update for Jolt tasks.
    PhysicsWorld();
    /// Destruct and free all Jolt objects. Rigid body nodes should not exist at this point, but if they do, remove the Jolt representation from all rigid body nodes still in the scene.
    ~PhysicsWorld();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Progress the simulation. When the timestep has passed, the actual physics simulation will be stepped.
    void Update(float deltaTime);
    /// Apply rigid body transforms to the scene nodes by interpolating according to the time accumulated so far. To be called after Update().
    void ApplyTransforms();
    /// Set gravity.
    void SetGravity(const Vector3& gravity);
    /// Set simulation timestep in seconds.
    void SetTimeStep(float timeStep);
    /// Set maximum simulation steps per Update() call. If exceeded, the simulation will slow down. Default 5.
    void SetMaxSteps(int maxSteps);

    /// Raycast into the physics world and return all hit results within distance, sorted by distance. Only rigid bodies whose collision layer matches the mask are considered.
    void Raycast(std::vector<PhysicsRaycastResult>& result, const Ray& ray, float maxDistance = M_MAX_FLOAT, unsigned collisionMask = M_MAX_UNSIGNED) const;
    /// Raycast into the physics world and return the closest result. The rigidBody member of the result is null if there was no hit. Only rigid bodies whose collision layer matches the mask are considered.
    PhysicsRaycastResult RaycastSingle(const Ray& ray, float maxDistance = M_MAX_FLOAT, unsigned collisionMask = M_MAX_UNSIGNED) const;
    /// Perform a sphere query into the physics world and return the rigid bodies within. Only rigid bodies whose collision layer matches the mask are considered.
    void SphereQuery(std::vector<RigidBody*>& result, const Sphere& sphere, unsigned collisionMask = M_MAX_UNSIGNED);

    /// Add debug geometry to be rendered. Visualize the physics world using Jolt debug draw functionality. No-op if Jolt was compiled without the debug renderer.
    void OnRenderDebug(DebugRenderer* debug);

    /// Return gravity.
    const Vector3& Gravity() const { return gravity; }
    /// Return the simulation timestep.
    float TimeStep() const { return timeStep; }
    /// Return maximum simulation steps per Update() call.
    int MaxSteps() const { return maxSteps; }
    /// Return number of simulation steps taken during the last Update() call. Rigid body contacts are only refreshed when at least one step was taken.
    int LastUpdateSteps() const { return lastUpdateSteps; }

    /// Applying transforms -flag. Set true to prevent hierarchical modifications signaling transform changes back to the simulation.
    bool inApplyTransforms;

private:
    /// Add a rigid body. Called by RigidBody.
    void AddRigidBody(RigidBody* body);
    /// Remove a rigid body. Its Jolt body is destroyed. Called by RigidBody.
    void RemoveRigidBody(RigidBody* body);
    /// Remove all rigid bodies.
    void RemoveAllRigidBodies();
    /// Queue a transform update from scene node to Jolt body.
    void QueueTransformUpdate(RigidBody* body);
    /// Queue a kinematic body to be moved towards its scene node transform during the next simulation steps.
    void QueueKinematicMove(RigidBody* body);
    /// Remove a rigid body from all update queues.
    void RemoveFromQueues(RigidBody* body);
    /// Apply queued transform updates to the Jolt bodies.
    void ProcessTransformUpdates();
    /// Store previous transforms of active bodies before a simulation step and mark them for interpolation.
    void StorePreviousTransforms(bool storeTransforms);
    /// Move queued kinematic bodies before a simulation step.
    void MoveKinematicBodies();
    /// Clear contacts which Jolt will report again before the first simulation step of an update.
    void ClearContacts();
    /// Collect contacts reported during a simulation step to the rigid bodies.
    void ProcessContacts();
    /// Add a contact to a rigid body, or merge with an existing contact to the same other body.
    void AddContact(RigidBody* rigidBody, RigidBody* otherBody, const Vector3& position, const Vector3& normal, bool isNew);
    /// Remove all contacts to and from a rigid body, when its Jolt body is being destroyed.
    void RemoveContacts(RigidBody* rigidBody);
    /// Return the Jolt group filter used for collision layer and mask filtering.
    static const JPH::GroupFilter* CollisionFilter();
    /// Return a cached convex or triangle mesh shape created from a model, or null on failure.
    JPH::Shape* ModelShape(Model* model, int lodLevel, bool convex);
    /// Return the Jolt body interface. Bodies are only accessed from the main thread outside the simulation step, so the non-locking interface is used.
    JPH::BodyInterface& JoltBodyInterface() const;

    /// Gravity.
    Vector3 gravity;
    /// Simulation timestep.
    float timeStep;
    /// Maximum simulation steps per update.
    int maxSteps;
    /// Simulation time accumulator.
    float timeAccumulator;
    /// Bodies added since last broadphase optimization.
    unsigned numBodiesAddedSinceOptimize;
    /// Simulation steps taken during the last update.
    int lastUpdateSteps;
    /// Jolt physics system.
    AutoPtr<JPH::PhysicsSystem> physicsSystem;
    /// Job system for Jolt using the WorkQueue.
    AutoPtr<JoltJobSystem> jobSystem;
    /// Jolt temporary allocator for the simulation step.
    AutoPtr<JPH::TempAllocator> tempAllocator;
    /// Mapping of object layers to broadphase layers.
    AutoPtr<JPH::BroadPhaseLayerInterfaceTable> broadPhaseLayerInterface;
    /// Object layer pair collision filter.
    AutoPtr<JPH::ObjectLayerPairFilterTable> objectLayerPairFilter;
    /// Object vs. broadphase layer collision filter.
    AutoPtr<JPH::ObjectVsBroadPhaseLayerFilterTable> objectVsBroadPhaseLayerFilter;
    /// Contact listener that buffers contacts during the simulation step.
    AutoPtr<PhysicsContactListener> contactListener;
    /// Cached Jolt shapes for models.
    AutoPtr<ModelShapeCache> modelShapes;
    /// All rigid bodies that have been added to the world.
    std::vector<RigidBody*> rigidBodies;
    /// Rigid bodies with pending transform updates from the scene node to the Jolt body.
    std::vector<RigidBody*> transformUpdates;
    /// Kinematic rigid bodies that are moving towards their target transform.
    std::vector<RigidBody*> kinematicMoves;
    /// Dynamic rigid bodies whose scene node transforms should be updated by interpolation.
    std::vector<RigidBody*> interpolatedBodies;
    /// Rigid bodies that have contacts stored.
    std::vector<RigidBody*> bodiesWithContacts;
    /// Cached %WorkQueue subsystem.
    WorkQueue* workQueue;
    /// Tasks for threaded rigid body transform updates.
    std::vector<AutoPtr<ApplyTransformsTask> > applyTransformsTasks;
};

/// Register Physics related object factories and attributes.
void RegisterPhysicsLibrary();
