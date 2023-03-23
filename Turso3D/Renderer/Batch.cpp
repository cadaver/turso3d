// For conditions of distribution and use, see copyright notice in License.txt

#include "Batch.h"
#include "GeometryNode.h"
#include "Material.h"

#include <algorithm>
#include <tracy/Tracy.hpp>

inline bool CompareBatchKeys(const Batch& lhs, const Batch& rhs)
{
    return lhs.sortKey < rhs.sortKey;
}

inline bool CompareBatchDistance(const Batch& lhs, const Batch& rhs)
{
    return lhs.distance > rhs.distance;
}

void BatchQueue::Clear()
{
    batches.clear();
}

void BatchQueue::Sort(std::vector<Matrix3x4>& instanceTransforms, BatchSortMode sortMode, bool convertToInstanced)
{
    ZoneScoped;

    switch (sortMode)
    {
    case SORT_STATE:
        for (auto it = batches.begin(); it < batches.end(); ++it)
        {
            unsigned short materialId = (unsigned short)((size_t)it->pass / sizeof(Pass));
            unsigned short geomId = (unsigned short)((size_t)it->geometry / sizeof(Geometry));

            it->sortKey = (((unsigned)materialId) << 16) | geomId;
        }
        std::sort(batches.begin(), batches.end(), CompareBatchKeys);
        break;

    case SORT_STATE_AND_DISTANCE:
        for (auto it = batches.begin(); it < batches.end(); ++it)
        {
            unsigned short materialId = it->pass->lastSortKey.second;
            unsigned short geomId = it->geometry->lastSortKey.second;
            
            it->sortKey = (((unsigned)materialId) << 16) | geomId;
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
        // Check if batch is static geometry and can be converted to instanced
        if (it->programBits)
            continue;

        size_t start = instanceTransforms.size();
        auto next = it + 1;

        if (next->pass == it->pass && next->geometry == it->geometry && !next->programBits)
        {
            // Convert to instances if at least one batch with same state found, then loop for more of the same
            it->instanceStart = (unsigned)start;
            it->programBits = SP_INSTANCED;
            instanceTransforms.push_back(*it->worldTransform);
            instanceTransforms.push_back(*next->worldTransform);
            ++next;

            for (; next < batches.end(); ++next)
            {
                if (next->pass == it->pass && next->geometry == it->geometry && !next->programBits)
                    instanceTransforms.push_back(*next->worldTransform);
                else
                    break;
            }

            // Finalize the conversion by writing instance count
            size_t count = instanceTransforms.size() - start;
            it->instanceCount = (unsigned)count;
            it += count - 1;
        }
    }
}
