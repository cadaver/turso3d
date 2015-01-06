// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Profiler.h"
#include "../Graphics/Texture.h"
#include "Batch.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

inline bool CompareBatchState(Batch& lhs, Batch& rhs)
{
    return lhs.sortKey < rhs.sortKey;
}

inline bool CompareBatchDistanceFrontToBack(Batch& lhs, Batch& rhs)
{
    return lhs.distance < rhs.distance;
}

inline bool CompareBatchDistanceBackToFront(Batch& lhs, Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

void BatchQueue::Clear()
{
    batches.Clear();
}

void BatchQueue::Sort(Vector<Matrix3x4>& instanceTransforms)
{
    {
        PROFILE(SortBatches);

        switch (sort)
        {
        case SORT_STATE:
            Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchState);
            break;

        case SORT_FRONT_TO_BACK:
            Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchDistanceFrontToBack);
            break;

        case SORT_BACK_TO_FRONT:
            Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchDistanceBackToFront);
            break;

        default:
            break;
        }
    }

    {
        PROFILE(BuildInstances);

        // Build instances where adjacent batches have same state
        Batch* start = nullptr;
        for (auto it = batches.Begin(), end = batches.End(); it != end; ++it)
        {
            Batch* current = &*it;
            if (start && current->type == GEOM_STATIC && current->pass == start->pass && current->geometry == start->geometry &&
                current->lights == start->lights)
            {
                if (start->type == GEOM_INSTANCED)
                {
                    instanceTransforms.Push(*current->worldMatrix);
                    ++start->instanceCount;
                }
                else
                {
                    // Begin new instanced batch
                    start->type = GEOM_INSTANCED;
                    size_t instanceStart = instanceTransforms.Size();
                    instanceTransforms.Push(*start->worldMatrix);
                    instanceTransforms.Push(*current->worldMatrix);
                    start->instanceStart = instanceStart; // Overwrites non-instance world matrix
                    start->instanceCount = 2; // Overwrites sort key / distance
                }
            }
            else
                start = (current->type == GEOM_STATIC) ? current : nullptr;
        }
    }
}

ShadowMap::ShadowMap()
{
    // Construct texture but do not define its size yet
    texture = new Texture();
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Clear()
{
    allocator.Reset(texture->Width(), texture->Height(), 0, 0, false);
    shadowViews.Clear();
    used = false;
}

void ShadowView::Clear()
{
    shadowQueue.Clear();
}

}
