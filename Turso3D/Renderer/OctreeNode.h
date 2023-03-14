// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/BoundingBox.h"
#include "../Scene/SpatialNode.h"

class Camera;
class DebugRenderer;
class Octree;
class Ray;
struct Octant;
struct RaycastResult;

/// Base class for scene nodes that insert themselves to the octree for rendering.
class OctreeNode : public SpatialNode
{
    friend class Octree;

    OBJECT(OctreeNode);

public:
    /// Construct.
    OctreeNode();
    /// Destruct. Remove self from the octree.
    ~OctreeNode();

    /// Register attributes.
    static void RegisterObject();

    /// Do processing before octree reinsertion, e.g. animation. Called by Octree in worker threads. Must be opted-in by setting NF_OCTREE_UPDATE_CALL flag.
    virtual void OnOctreeUpdate(unsigned short frameNumber);
    /// Prepare object for rendering. Reset framenumber and calculate distance from camera. Called by Renderer in worker threads. Return false if should not render.
    virtual bool OnPrepareRender(unsigned short frameNumber, Camera* camera);
    /// Perform ray test on self and add possible hit to the result vector.
    virtual void OnRaycast(std::vector<RaycastResult>& dest, const Ray& ray, float maxDistance);
    /// Add debug geometry to be rendered. Default implementation draws the bounding box.
    virtual void OnRenderDebug(DebugRenderer* debug);

    /// Set whether to cast shadows. Default false on both lights and geometries.
    void SetCastShadows(bool enable);
    /// Set max distance for rendering. 0 is unlimited.
    void SetMaxDistance(float distance);
    
    /// Return world space bounding box. Update if necessary. 
    const BoundingBox& WorldBoundingBox() const { if (TestFlag(NF_BOUNDING_BOX_DIRTY)) OnWorldBoundingBoxUpdate(); return worldBoundingBox; }
    /// Return whether casts shadows.
    bool CastShadows() const { return TestFlag(NF_CAST_SHADOWS); }
    /// Return current octree this node resides in.
    Octree* GetOctree() const { return octree; }
    /// Return current octree octant this node resides in.
    Octant* GetOctant() const { return octant; }
    /// Return distance from camera in the current view.
    float Distance() const { return distance; }
    /// Return max distance for rendering, or 0 for unlimited.
    float MaxDistance() const { return maxDistance; }
    /// Return last frame number when was visible. The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastFrameNumber() const { return lastFrameNumber; }
    /// Return last frame number when was reinserted to octree (moved or animated.) The frames are counted by Renderer internally and have no significance outside it.
    unsigned short LastUpdateFrameNumber() const { return lastUpdateFrameNumber; }
    /// Check whether is marked in view this frame.
    bool InView(unsigned short frameNumber) { return lastFrameNumber == frameNumber; }

    /// Check whether was in view last frame, compared to the current.
    bool WasInView(unsigned short frameNumber)
    {
        unsigned short previousFrameNumber = frameNumber - 1;
        if (!previousFrameNumber)
            --previousFrameNumber;
        return lastFrameNumber == previousFrameNumber;
    }

protected:
    /// Search for an octree from the scene root and add self to it.
    void OnSceneSet(Scene* newScene, Scene* oldScene) override;
    /// Handle the transform matrix changing.
    void OnTransformChanged() override;
    /// Handle the enabled status changing.
    void OnEnabledChanged(bool newEnabled) override;
    /// Recalculate the world space bounding box.
    virtual void OnWorldBoundingBoxUpdate() const;

    /// World space bounding box.
    mutable BoundingBox worldBoundingBox;
    /// Last frame number when was visible.
    unsigned short lastFrameNumber;
    /// Last frame number when was reinserted to octree or other change (LOD etc.) happened
    unsigned short lastUpdateFrameNumber;
    /// Distance from camera in the current view.
    float distance;
    /// Max distance for rendering.
    float maxDistance;
    /// Current octree for octree nodes.
    Octree* octree;
    /// Current octant in octree for octree nodes.
    Octant* octant;

private:
    /// Remove from the current octree.
    void RemoveFromOctree();
};
