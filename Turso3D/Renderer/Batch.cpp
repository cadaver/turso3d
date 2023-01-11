#include "../Graphics/FrameBuffer.h"
#include "../Graphics/Texture.h"
#include "Batch.h"

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

void BatchQueue::Sort(std::vector<Matrix3x4>& instanceTransforms, bool sortByState, bool convertToInstanced)
{
    if (sortByState)
    {
        for (auto it = batches.begin(); it < batches.end(); ++it)
            it->SetStateSortKey();

        std::sort(batches.begin(), batches.end(), CompareBatchKeys);
    }
    else
    {
        std::sort(batches.begin(), batches.end(), CompareBatchDistance);
    }

    if (!convertToInstanced || batches.size() < 2)
        return;

    for (auto it = batches.begin(); it < batches.end() - 1; ++it)
    {
        Batch& current = *it;
        ShaderProgram* origProgram = current.program;
        // Check if batch can be converted (static geometry)
        if (origProgram->programBits & SP_GEOMETRYBITS)
            continue;

        bool hasInstances = false;
        size_t start = instanceTransforms.size();

        for (auto it2 = it + 1; it2 < batches.end(); ++it2)
        {
            Batch& next = *it2;

            if (next.program == origProgram && next.lightPass == current.lightPass && next.pass == current.pass && next.geometry == current.geometry)
            {
                if (!hasInstances)
                {
                    current.program = current.pass->GetShaderProgram(origProgram->programBits | GEOM_INSTANCED);
                    instanceTransforms.push_back(*current.worldTransform);
                    hasInstances = true;
                }
                
                instanceTransforms.push_back(*next.worldTransform);
            }
            else
                break;
        }

        if (hasInstances)
        {
            size_t count = instanceTransforms.size() - start;
            current.instanceRange[0] = start;
            current.instanceRange[1] = count;
            batches.erase(it + 1, it + count);
        }
    }
}
