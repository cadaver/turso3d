// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/BoundingBox.h"
#include "SpatialNode.h"

namespace Turso3D
{

class Octree;
class Ray;
struct Octant;
struct RaycastResult;

/// Base class for nodes that insert themselves to the octree for rendering.
class OctreeNode : public SpatialNode
{
    OBJECT(OctreeNode);

    friend class Octree;

public:
    /// Construct.
    OctreeNode();
    /// Destruct. Remove self from the octree.
    ~OctreeNode();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Perform ray test on self and add possible hit to the result vector.
    virtual void OnRaycast(Vector<RaycastResult>& dest, const Ray& ray, float maxDistance);
    
    /// Return the current octree this node resides in.
    Octree* CurrentOctree() const { return octree; }
    /// Return the current octree octant this node resides in.
    Octant* CurrentOctant() const { return octant; }
    /// Return the world bounding box. Update if necessary.
    const BoundingBox& WorldBoundingBox() const { if (TestFlag(NF_BOUNDING_BOX_DIRTY)) OnWorldBoundingBoxUpdate(); return worldBoundingBox; }

protected:
    /// Search for an octree from the scene root and add self to it.
    virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
    /// Handle the transform matrix changing.
    virtual void OnTransformChanged();
    /// Recalculate the world bounding box.
    virtual void OnWorldBoundingBoxUpdate() const;

private:
    /// Remove from the current octree.
    void RemoveFromOctree();

    /// World bounding box.
    mutable BoundingBox worldBoundingBox;
    /// Current octree.
    Octree* octree;
    /// Current octree octant.
    Octant* octant;
};

}
