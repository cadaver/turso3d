#include "../Graphics/FrameBuffer.h"
#include "../Graphics/Texture.h"
#include "../Graphics/VertexBuffer.h"
#include "Batch.h"
#include "GeometryNode.h"
#include "Material.h"

#include <algorithm>

inline bool CompareBatchKeys(const Batch& lhs, const Batch& rhs)
{
    return lhs.sortKey < rhs.sortKey;
}

inline bool CompareBatchDistance(const Batch& lhs, const Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

ShadowMap::ShadowMap()
{
    // Construct texture but do not define its size yet
    texture = new Texture();
    fbo = new FrameBuffer();
}

ShadowMap::~ShadowMap()
{
}

void ShadowMap::Clear()
{
    allocator.Reset(texture->Width(), texture->Height(), 0, 0, false);
    shadowViews.clear();
    freeQueueIdx = 0;
}

void BatchQueue::Clear()
{
    batches.clear();
}

void BatchQueue::Sort(std::vector<Matrix3x4>& instanceTransforms, BatchSortMode sortMode, bool convertToInstanced)
{
    switch (sortMode)
    {
    case SORT_STATE:
        for (auto it = batches.begin(); it < batches.end(); ++it)
        {
            unsigned short lightId = (unsigned short)((size_t)it->lightPass / sizeof(LightPass));
            unsigned short materialId = (unsigned short)((size_t)it->pass / sizeof(Pass));
            unsigned short geomId = (unsigned short)((size_t)it->geometry / sizeof(Geometry));

            it->sortKey = (((unsigned long long)lightId) << 32) | (((unsigned long long)materialId) << 16) | geomId;
        }
        std::sort(batches.begin(), batches.end(), CompareBatchKeys);
        break;

    case SORT_STATE_AND_DISTANCE:
        for (auto it = batches.begin(); it < batches.end(); ++it)
        {
            unsigned short lightId = it->lightPass ? it->lightPass->lastSortKey.second : 0;
            unsigned short materialId = it->pass->lastSortKey.second;
            unsigned short geomId = it->geometry->lastSortKey.second;
                
            it->sortKey = (((unsigned long long)lightId) << 32) | (((unsigned long long)materialId) << 16) | geomId;
        }
        std::sort(batches.begin(), batches.end(), CompareBatchKeys);
        break;

    case SORT_DISTANCE:
        std::sort(batches.begin(), batches.end(), CompareBatchDistance);
        break;
    }

    if (!convertToInstanced || batches.size() < 2)
        return;

    for (auto it = batches.begin(); it < batches.end() - 1; ++it)
    {
        // Check if batch can be converted (static geometry)
        if (it->programBits & SP_GEOMETRYBITS)
            continue;

        bool hasInstances = false;
        size_t start = instanceTransforms.size();

        for (auto next = it + 1; next < batches.end(); ++next)
        {
            if (next->programBits == it->programBits && next->lightPass == it->lightPass && next->pass == it->pass && next->geometry == it->geometry)
            {
                if (!hasInstances)
                {
                    instanceTransforms.push_back(*it->worldTransform);
                    hasInstances = true;
                }
                
                instanceTransforms.push_back(*next->worldTransform);
            }
            else
                break;
        }

        if (hasInstances)
        {
            size_t count = instanceTransforms.size() - start;
            it->programBits |= GEOM_INSTANCED;
            it->instanceRange[0] = (unsigned)start;
            it->instanceRange[1] = (unsigned)count;
            batches.erase(it + 1, it + count);
        }
    }
}
