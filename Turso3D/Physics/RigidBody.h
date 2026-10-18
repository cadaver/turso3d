// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Scene/SpatialNode.h"

class Model;
class PhysicsWorld;
class RigidBody;

namespace JPH
{
    class Body;
}

static const unsigned DEFAULT_COLLISION_LAYER = 1;
static const unsigned DEFAULT_COLLISION_MASK = M_MAX_UNSIGNED;

/// Collision shape type of a rigid body.
enum CollisionShapeType
{
    SHAPE_NONE = 0,
    SHAPE_BOX,
    SHAPE_SPHERE,
    SHAPE_CAPSULE,
    SHAPE_CYLINDER,
    SHAPE_CONVEX,
    SHAPE_TRIANGLEMESH
};

/// Contact between two rigid bodies, reported after a physics world update.
struct PhysicsContact
{
    /// The other rigid body.
    RigidBody* otherBody;
    /// World-space contact position.
    Vector3 position;
    /// World-space contact normal, pointing from the other body towards this body.
    Vector3 normal;
    /// Whether the bodies were not touching during the previous simulation step.
    bool isNew;
};

/// Rigid body scene node. The Jolt body exists when the node is enabled, has a collision shape and is in a scene that contains a physics world. Body properties should only be accessed from the main thread.
class RigidBody : public SpatialNode
{
    OBJECT(RigidBody);

    friend class PhysicsWorld;
    friend struct ApplyTransformsTask;

public:
    /// Construct.
    RigidBody();
    /// Destruct.
    ~RigidBody();

    /// Register attributes and object factory.
    static void RegisterObject();

    /// Set mass of the body. Zero mass (default) makes a non-kinematic body static.
    void SetMass(float mass);
    /// Disable / enable kinematic mode. Default is false (dynamic). Kinematic bodies are moved by setting the scene node transform.
    void SetKinematic(bool enable);
    /// Set a box collision shape.
    void SetBoxShape(const Vector3& size);
    /// Set a capsule collision shape along the Y axis. X is the diameter and Y the total height. Z is unused.
    void SetCapsuleShape(const Vector3& size);
    /// Set a cylinder collision shape along the Y axis. X is the diameter and Y the height. Z is unused.
    void SetCylinderShape(const Vector3& size);
    /// Set a sphere collision shape.
    void SetSphereShape(float diameter);
    /// Set a convex collision shape derived from a Model resource. Default LOD is the coarsest while 0 would be the most detailed.
    void SetConvexShape(Model* model, const Vector3& modelScale = Vector3::ONE, int lodLevel = M_MAX_INT);
    /// Set a triangle mesh collision shape derived from a Model resource. Default LOD is the coarsest while 0 would be the most detailed. Should be used only with static or kinematic bodies. 
    void SetTriangleShape(Model* model, const Vector3& modelScale = Vector3::ONE, int lodLevel = M_MAX_INT);
    /// Remove the collision shape. This also removes the body from the simulation.
    void RemoveShape();
    /// Set collision layer bits. Two bodies collide if each one's layer matches the other's mask. Default 1.
    void SetCollisionLayer(unsigned layer);
    /// Set collision mask bits. Default all bits set, which collides with all layers.
    void SetCollisionMask(unsigned mask);

    /// Forcibly set the body's linear velocity.
    void SetLinearVelocity(const Vector3& linearVelocity);
    /// Forcibly set the body's angular velocity.
    void SetAngularVelocity(const Vector3& angularVelocity);
    /// Add a force to the body. Forces are cleared after each simulation step.
    void AddForce(const Vector3& force);
    /// Add a force to the body at world-space offset from the body's position. Forces are cleared after each simulation step.
    void AddForce(const Vector3& force, const Vector3& offset);
    /// Add a torque to the body. Torques are cleared after each simulation step.
    void AddTorque(const Vector3& torque);
    /// Add an impulse to the body.
    void AddImpulse(const Vector3& impulse);
    /// Wake up the body if it is sleeping.
    void Activate();

    /// Return linear velocity.
    Vector3 LinearVelocity() const;
    /// Return angular velocity.
    Vector3 AngularVelocity() const;
    /// Return mass.
    float Mass() const { return mass; }
    /// Return whether is kinematic.
    bool IsKinematic() const { return kinematic; }
    /// Return whether is static (zero mass and not kinematic.)
    bool IsStatic() const { return !kinematic && mass <= 0.0f; }
    /// Return whether the body is active (not sleeping) in the simulation.
    bool IsActive() const;
    /// Return collision shape type.
    CollisionShapeType ShapeType() const { return shapeType; }
    /// Return collision shape size for primitive shapes, or scale for model shapes. For sphere, all components are the diameter.
    const Vector3& ShapeSize() const { return shapeSize; }
    /// Return collision shape model.
    Model* ShapeModel() const;
    /// Return collision layer bits.
    unsigned CollisionLayer() const { return collisionLayer; }
    /// Return collision mask bits.
    unsigned CollisionMask() const { return collisionMask; }
    /// Return contacts to other rigid bodies from the simulation steps of the last PhysicsWorld::Update() which stepped the simulation. There is one contact per other body.
    /// Contacts between two inactive (sleeping or static) bodies are not reported by Jolt, so they are retained from the time the bodies were last active.
    const std::vector<PhysicsContact>& Contacts() const { return contacts; }
    /// Return the physics world.
    PhysicsWorld* GetPhysicsWorld() const { return world; }

    /// Apply a new world transform from the physics simulation and dirty possible child node world transforms.
    void ApplyTransform(const Vector3& newPosition, const Quaternion& newRotation);

protected:
    /// Assign to a scene. Search for a physics world from the scene root and add self to it.
    void OnSceneSet(Scene* newScene, Scene* oldScene) override;
    /// Handle the transform changing from outside the physics simulation. Apply position / rotation to the Jolt body.
    void OnTransformChanged() override;
    /// Handle the enabled status changing. When disabled, the body should not take part in the physics simulation.
    void OnEnabledChanged(bool newEnabled) override;

private:
    /// Set the shape parameters and recreate the body.
    void SetShape(CollisionShapeType type, const Vector3& size, Model* model = nullptr, int lodLevel = M_MAX_INT);
    /// Create the Jolt body if all requirements are fulfilled. Preserve the given velocities if dynamic.
    void CreateBody(const Vector3& linearVelocity = Vector3::ZERO, const Vector3& angularVelocity = Vector3::ZERO);
    /// Destroy the Jolt body if exists.
    void ReleaseBody();
    /// Recreate the Jolt body after a property change, preserving the velocities.
    void RecreateBody();
    /// Apply the scene node transform to the Jolt body. Called by PhysicsWorld.
    void ApplyNodeTransform();
    /// Apply collision layer and mask to the Jolt body.
    void ApplyCollisionFilter();
    /// Set shape attribute. Used in serialization.
    void SetShapeAttr(const JSONValue& value);
    /// Return shape attribute. Used in serialization.
    JSONValue ShapeAttr() const;

    /// Physics world.
    PhysicsWorld* world;
    /// Jolt body.
    JPH::Body* body;
    /// Contacts from the last simulation update.
    std::vector<PhysicsContact> contacts;
    /// Collision shape model.
    SharedPtr<Model> shapeModel;
    /// Collision shape model LOD level, 0 = most detailed.
    int shapeModelLod;
    /// Collision shape size or scale.
    Vector3 shapeSize;
    /// World scale that was used when creating the Jolt shape.
    Vector3 shapeWorldScale;
    /// Previous physics position for interpolation.
    Vector3 prevPosition;
    /// Previous physics rotation for interpolation.
    Quaternion prevRotation;
    /// Mass.
    float mass;
    /// Collision layer bits.
    unsigned collisionLayer;
    /// Collision mask bits.
    unsigned collisionMask;
    /// Index in the physics world's list of bodies with contacts, or -1 if not in the list.
    int contactListIndex;
    /// Collision shape type.
    CollisionShapeType shapeType;
    /// Kinematic flag.
    bool kinematic;
    /// Queued for transform update flag.
    bool transformUpdateQueued;
    /// Queued for kinematic move flag.
    bool kinematicMoveQueued;
    /// In interpolation list flag.
    bool interpolated;
};
