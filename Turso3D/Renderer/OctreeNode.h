// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/BoundingBox.h"
#include "../Scene/SpatialNode.h"

class Camera;
class DebugRenderer;
class Drawable;
class Octant;
class Octree;
class Ray;
struct RaycastResult;

static const unsigned short DF_STATIC_GEOMETRY = 0x0;
static const unsigned short DF_SKINNED_GEOMETRY = 0x1;
static const unsigned short DF_INSTANCED_GEOMETRY = 0x2;
static const unsigned short DF_CUSTOM_GEOMETRY = 0x3;
static const unsigned short DF_GEOMETRY_TYPE_BITS = 0x3;
static const unsigned short DF_LIGHT = 0x4;
static const unsigned short DF_GEOMETRY = 0x8;
static const unsigned short DF_STATIC = 0x10;
static const unsigned short DF_CAST_SHADOWS = 0x20;
static const unsigned short DF_UPDATE_INVISIBLE = 0x40;
static const unsigned short DF_HAS_LOD_LEVELS = 0x80;
static const unsigned short DF_OCTREE_UPDATE_CALL = 0x100;
static const unsigned short DF_WORLD_TRANSFORM_DIRTY = 0x200;
static const unsigned short DF_BOUNDING_BOX_DIRTY = 0x400;
static const unsigned short DF_OCTREE_REINSERT_QUEUED = 0x800;

/// Common base class for renderable scene objects and occluders.
class OctreeNodeBase : public SpatialNode
{
    friend class Octree;

public:
    /// Construct.
    OctreeNodeBase();

protected:
    /// Handle the layer changing.
    void OnLayerChanged(unsigned char newLayer) override;

    /// Current octree.
    Octree* octree;
    /// This node's drawable.
    Drawable* drawable;
};

/// Base class for objects that are inserted to the octree for rendering. These are managed by their scene node. Inserting drawables instead of scene nodes helps to keep the rendering-critical information more tightly packed in memory.
class Drawable
{
    friend class Octree;
    friend class OctreeNode;

public:
    /// Construct.
    Drawable();
    /// Destruct.
    virtual ~Drawable();

    /// Recalculate the world space bounding box.
    virtual void OnWorldBoundingBoxUpdate() const;
    /// Do processing before octree reinsertion, e.g. animation. Called by Octree in worker threads. Must be opted-in by setting DF_OCTREE_UPDATE_CALL flag.
    virtual void OnOctreeUpdate(unsigned short frameNumber);
    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer in worker threads. Return false if should not render.
    virtual bool OnPrepareRender(unsigned short frameNumber, Camera* camera);
    /// Perform ray test on self and add possible hit to the result vector.
    virtual void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance);
    /// Add debug geometry to be rendered. Default implementation draws the bounding box.
    virtual void OnRenderDebug(DebugRenderer* debug);

    /// Set the owner node.
    void SetOwner(OctreeNodeBase* owner);
    /// Set the layer.
    void SetLayer(unsigned char newLayer);

    /// Return flags.
    unsigned short Flags() const { return flags; }
    /// Return bitmask corresponding to layer.
    unsigned LayerMask() const { return 1 << layer; }
    /// Return the owner node.
    OctreeNodeBase* Owner() const { return owner; }
    /// Return current octree octant this drawable resides in.
    Octant* GetOctant() const { return octant; }
    /// Return distance from camera in the current view.
    float Distance() const { return distance; }
    /// Return max distance for rendering, or 0 for unlimited.
    float MaxDistance() const { return maxDistance; }
    /// Return whether is static.
    bool IsStatic() const { return TestFlag(DF_STATIC); }
    /// Return last frame number when was visible. The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastFrameNumber() const { return lastFrameNumber; }
    /// Return last frame number when was reinserted to octree (moved or animated.) The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastUpdateFrameNumber() const { return lastUpdateFrameNumber; }
    /// Check whether is marked in view this frame.
    bool InView(unsigned short frameNumber) { return lastFrameNumber == frameNumber; }
    /// Return position in world space.
    Vector3 WorldPosition() const { return WorldTransform().Translation(); }
    /// Return rotation in world space.
    Quaternion WorldRotation() const { return WorldTransform().Rotation(); }
    /// Return forward direction in world space.
    Vector3 WorldDirection() const { return WorldRotation() * Vector3::FORWARD; }
    /// Return scale in world space. As it is calculated from the world transform matrix, it may not be meaningful or accurate in all cases.
    Vector3 WorldScale() const { return WorldTransform().Scale(); }

    /// Return world space bounding box. Update if necessary.
    const BoundingBox& WorldBoundingBox() const
    {
        if (TestFlag(DF_BOUNDING_BOX_DIRTY))
        {
            OnWorldBoundingBoxUpdate();
            SetFlag(DF_BOUNDING_BOX_DIRTY, false);
        }
        return worldBoundingBox;
    }

    /// Return world transform matrix. Update if necessary
    const Matrix3x4& WorldTransform() const
    {
        if (TestFlag(DF_WORLD_TRANSFORM_DIRTY))
        {
            SetFlag(DF_WORLD_TRANSFORM_DIRTY, false);
            // Update the shared world transform as necessary, then return
            return owner->WorldTransform();
        }
        else
            return *worldTransform;
    }

    /// Check whether was in view last frame, compared to the current.
    bool WasInView(unsigned short frameNumber) const
    {
        unsigned short previousFrameNumber = frameNumber - 1;
        if (!previousFrameNumber)
            --previousFrameNumber;
        return lastFrameNumber == previousFrameNumber;
    }

    /// Set bit flag. Called internally.
    void SetFlag(unsigned short bit, bool set) const { if (set) flags |= bit; else flags &= ~bit; }
    /// Test bit flag. Called internally.
    bool TestFlag(unsigned short bit) const { return (flags & bit) != 0; }

protected:
    /// World space bounding box.
    mutable BoundingBox worldBoundingBox;
    /// Owner scene node's world transform matrix.
    Matrix3x4* worldTransform;
    /// Current octree octant.
    Octant* octant;
    /// %Drawable flags. Used to hold several boolean values to reduce memory use.
    mutable unsigned short flags;
    /// Layer number. Copy of the node layer.
    unsigned char layer;
    /// Last frame number when was visible.
    unsigned short lastFrameNumber;
    /// Last frame number when was reinserted to octree or other change (LOD etc.) happened.
    unsigned short lastUpdateFrameNumber;
    /// Distance from camera in the current view.
    float distance;
    /// Max distance for rendering.
    float maxDistance;
    /// Owner scene node.
    OctreeNodeBase* owner;
};

/// Base class for scene nodes that insert drawables to the octree for rendering.
class OctreeNode : public OctreeNodeBase
{
    OBJECT(OctreeNode);

public:
    /// Register attributes.
    static void RegisterObject();

    /// Set whether is static. Used for optimizations. A static node should not move after scene load. Default false.
    void SetStatic(bool enable);
    /// Set whether to cast shadows. Default false on both lights and geometries.
    void SetCastShadows(bool enable);
    /// Set whether to update animation when invisible. Default false for better performance.
    void SetUpdateInvisible(bool enable);
    /// Set max distance for rendering. 0 is unlimited.
    void SetMaxDistance(float distance);
    
    /// Return drawable's world space bounding box. Update if necessary. 
    const BoundingBox& WorldBoundingBox() const { return drawable->WorldBoundingBox(); }
    /// Return whether is static.
    bool IsStatic() const { return drawable->TestFlag(DF_STATIC); }
    /// Return whether casts shadows.
    bool CastShadows() const { return drawable->TestFlag(DF_CAST_SHADOWS); }
    /// Return whether updates animation when invisible. Not relevant for non-animating geometry.
    bool UpdateInvisible() const { return drawable->TestFlag(DF_UPDATE_INVISIBLE); }
    /// Return current octree this node resides in.
    Octree* GetOctree() const { return octree; }
    /// Return the drawable for internal use.
    Drawable* GetDrawable() const { return drawable; }
    /// Return distance from camera in the current view.
    float Distance() const { return drawable->Distance(); }
    /// Return max distance for rendering, or 0 for unlimited.
    float MaxDistance() const { return drawable->MaxDistance(); }
    /// Return last frame number when was visible. The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastFrameNumber() const { return drawable->LastFrameNumber(); }
    /// Return last frame number when was reinserted to octree (moved or animated.) The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastUpdateFrameNumber() const { return drawable->LastUpdateFrameNumber(); }
    /// Check whether is marked in view this frame.
    bool InView(unsigned short frameNumber) { return drawable->InView(frameNumber); }
    /// Check whether was in view last frame, compared to the current.
    bool WasInView(unsigned short frameNumber) const { return drawable->WasInView(frameNumber); }

protected:
    /// Search for an octree from the scene root and add self to it.
    void OnSceneSet(Scene* newScene, Scene* oldScene) override;
    /// Handle the transform matrix changing. Queue octree reinsertion for the drawable.
    void OnTransformChanged() override;
    /// Handle the bounding box changing. Only queue octree reinsertion, does not dirty the node hierarchy.
    void OnBoundingBoxChanged();
    /// Handle the enabled status changing.
    void OnEnabledChanged(bool newEnabled) override;
    /// Remove from the current octree.
    void RemoveFromOctree();
};
