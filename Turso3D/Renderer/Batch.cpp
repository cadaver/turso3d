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

inline bool CompareBatchDistance(Batch& lhs, Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

void BatchQueue::Clear()
{
    batches.Clear();
    instanceDatas.Clear();
    instanceLookup.Clear();
}

void BatchQueue::AddBatch(Batch& batch, GeometryNode* node, bool isAdditive)
{
    if (sort == SORT_STATE)
    {
        if (batch.type == GEOM_STATIC)
        {
            batch.type = GEOM_INSTANCED;
            batch.CalculateSortKey(isAdditive);

            // Check if instance batch already exists
            auto iIt = instanceLookup.Find(batch.sortKey);
            if (iIt != instanceLookup.End())
                instanceDatas[iIt->second].worldMatrices.Push(&node->WorldTransform());
            else
            {
                // Begin new instanced batch
                size_t newInstanceDataIndex = instanceDatas.Size();
                instanceLookup[batch.sortKey] = newInstanceDataIndex;
                batch.instanceDataIndex = newInstanceDataIndex;
                batches.Push(batch);
                instanceDatas.Resize(newInstanceDataIndex + 1);
                InstanceData& newInstanceData = instanceDatas.Back();
                newInstanceData.skipBatches = false;
                newInstanceData.worldMatrices.Push(&node->WorldTransform());
            }
        }
        else
        {
            batch.worldMatrix = &node->WorldTransform();
            batch.CalculateSortKey(isAdditive);
            batches.Push(batch);
        }
    }
    else
    {
        batch.worldMatrix = &node->WorldTransform();
        batch.distance = node->Distance();
        // Push additive passes slightly to front to make them render after base passes
        if (isAdditive)
            batch.distance *= 0.999999f;
        batches.Push(batch);
    }
}

void BatchQueue::Sort()
{
    PROFILE(SortBatches);

    if (sort == SORT_STATE)
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchState);
    else
    {
        Turso3D::Sort(batches.Begin(), batches.End(), CompareBatchDistance);

        // After sorting batches by distance, we need a separate step to build instances if adjacent batches have the same state
        Batch* start = nullptr;
        for (auto it = batches.Begin(), end = batches.End(); it != end; ++it)
        {
            Batch* current = &*it;
            if (start && current->type == GEOM_STATIC && current->pass == start->pass && current->geometry == start->geometry &&
                current->lights == start->lights)
            {
                if (start->type == GEOM_INSTANCED)
                    instanceDatas[start->instanceDataIndex].worldMatrices.Push(current->worldMatrix);
                else
                {
                    // Begin new instanced batch
                    start->type = GEOM_INSTANCED;
                    size_t newInstanceDataIndex = instanceDatas.Size();
                    instanceDatas.Resize(newInstanceDataIndex + 1);
                    InstanceData& newInstanceData = instanceDatas.Back();
                    newInstanceData.skipBatches = true;
                    newInstanceData.worldMatrices.Push(start->worldMatrix);
                    newInstanceData.worldMatrices.Push(current->worldMatrix);
                    start->instanceDataIndex = newInstanceDataIndex; // Overwrites the non-instanced world matrix
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
    shadowCasters.Clear();
    shadowQueue.Clear();
}

}
