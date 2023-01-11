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
        ShaderProgram* origProgram = it->program;
        // Check if batch can be converted (static geometry)
        if (origProgram->programBits & SP_GEOMETRYBITS)
            continue;

        bool hasInstances = false;
        size_t start = instanceTransforms.size();

        for (auto next = it + 1; next < batches.end(); ++next)
        {
            if (next->program == origProgram && next->lightPass == it->lightPass && next->pass == it->pass && next->geometry == it->geometry)
            {
                if (!hasInstances)
                {
                    it->program = it->pass->GetShaderProgram(origProgram->programBits | GEOM_INSTANCED);
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
            it->instanceRange[0] = start;
            it->instanceRange[1] = count;
            batches.erase(it + 1, it + count);
        }
    }
}
