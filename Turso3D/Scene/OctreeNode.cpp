// For conditions of distribution and use, see copyright notice in License.txt

#include "Octree.h"
#include "OctreeNode.h"
#include "Scene.h"

namespace Turso3D
{

OctreeNode::OctreeNode() :
    octree(0),
    octant(0)
{
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);
}

OctreeNode::~OctreeNode()
{
    RemoveFromOctree();
}

void OctreeNode::RegisterObject()
{
    RegisterFactory<OctreeNode>();
    CopyBaseAttributes<OctreeNode, SpatialNode>();
}

void OctreeNode::OnSceneSet(Scene* newScene, Scene*)
{
    /// Remove from current octree if any
    RemoveFromOctree();

    if (newScene)
    {
        // Octree must be attached to the scene root as a child
        octree = newScene->FindChild<Octree>();
        // Transform may not be final yet. Schedule update but do not insert into octree yet
        if (octree)
            octree->QueueUpdate(this);
    }
}

void OctreeNode::OnTransformChanged()
{
    SpatialNode::OnTransformChanged();
    SetFlag(NF_BOUNDING_BOX_DIRTY, true);

    if (!TestFlag(NF_OCTREE_UPDATE_QUEUED) && octree)
        octree->QueueUpdate(this);
}

void OctreeNode::OnWorldBoundingBoxUpdate() const
{
    // The OctreeNode base class does not have a defined size, so represent as a point
    worldBoundingBox.Define(WorldPosition());
    SetFlag(NF_BOUNDING_BOX_DIRTY, false);
}

void OctreeNode::RemoveFromOctree()
{
    if (octree)
    {
        octree->RemoveNode(this);
        octree = 0;
    }
}

}