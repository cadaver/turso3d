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

void Batch::SetStateSortKey()
{
    unsigned short lightId = lightPass ? lightPass->lastSortKey.second : 0;
    unsigned short materialId = pass->lastSortKey.second;
    unsigned short geomId = geometry->lastSortKey.second;

    // If uses a combined vertex buffer, add its key to light sorting key to further reduce state changes
    if (geometry->useCombined)
        lightId += geometry->vertexBuffer->lastSortKey.second;

    sortKey = (((unsigned long long)lightId) << 32) |
        (((unsigned long long)materialId) << 16) |
        ((unsigned long long)geomId);
}

void BatchQueue::Clear()
{
    batches.clear();
}

void BatchQueue::Sort(std::vector<Matrix3x4>& instanceTransforms, bool sortByState, bool convertToInstanced)
{
    if (sortByState)
        std::sort(batches.begin(), batches.end(), CompareBatchKeys);
    else
    {
        std::sort(batches.begin(), batches.end(), CompareBatchDistance);
    }

    if (!convertToInstanced || batches.size() < 2)
        return;

    for (size_t i = 0; i < batches.size() - 1;)
    {
        Batch& current = batches[i];
        ShaderProgram* origProgram = current.program;
        // Check if batch can be converted (static geometry)
        if (origProgram->programBits & SP_GEOMETRYBITS)
        {
            ++i;
            continue;
        }

        bool hasInstances = false;

        for (size_t j = i + 1; j < batches.size(); ++j)
        {
            Batch& next = batches[j];

            if (next.program == origProgram && next.lightPass == current.lightPass && next.pass == current.pass && next.geometry == current.geometry)
            {
                if (hasInstances)
                {
                    instanceTransforms.push_back(*next.worldTransform);
                    ++current.instanceRange[1];
                }
                else
                {
                    unsigned char newProgramBits = origProgram->programBits | GEOM_INSTANCED;
                    ShaderProgram* newProgram = current.pass->GetShaderProgram(newProgramBits);
                    hasInstances = true;
                    unsigned numInstancesNow = (unsigned)instanceTransforms.size();

                    current.program = newProgram;
                    instanceTransforms.push_back(*current.worldTransform);
                    instanceTransforms.push_back(*next.worldTransform);
                    current.instanceRange[0] = numInstancesNow;
                    current.instanceRange[1] = 2;
                }
            }
            else
                break;
        }

        if (hasInstances)
        {
            batches.erase(batches.begin() + i + 1, batches.begin() + i + current.instanceRange[1]);
            ++i;
        }
        else
            ++i;
    }
}
