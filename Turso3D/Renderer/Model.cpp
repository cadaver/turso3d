// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Scene/Node.h"
#include "GeometryNode.h"
#include "Material.h"
#include "Model.h"

#include <tracy/Tracy.hpp>

// Vertex and index allocation for the combined model buffers
static const size_t COMBINEDBUFFER_VERTICES = 384 * 1024;
static const size_t COMBINEDBUFFER_INDICES = 1024 * 1024;

// Bone bounding box size required to contribute to bounding box recalculation
static const float BONE_SIZE_THRESHOLD = 0.05f;

std::map<unsigned, std::vector<WeakPtr<CombinedBuffer> > > CombinedBuffer::buffers;

CombinedBuffer::CombinedBuffer(const std::vector<VertexElement>& elements) :
    usedVertices(0),
    usedIndices(0)
{
    vertexBuffer = new VertexBuffer();
    vertexBuffer->Define(USAGE_DEFAULT, COMBINEDBUFFER_VERTICES, elements);
    indexBuffer = new IndexBuffer();
    indexBuffer->Define(USAGE_DEFAULT, COMBINEDBUFFER_INDICES, sizeof(unsigned));
}

bool CombinedBuffer::FillVertices(size_t numVertices, const void* data)
{
    if (usedVertices + numVertices > vertexBuffer->NumVertices())
        return false;

    vertexBuffer->SetData(usedVertices, numVertices, data);
    usedVertices += numVertices;
    return true;
}

bool CombinedBuffer::FillIndices(size_t numIndices, const void* data)
{
    if (usedIndices + numIndices > indexBuffer->NumIndices())
        return false;

    indexBuffer->SetData(usedIndices, numIndices, data);
    usedIndices += numIndices;
    return true;
}

CombinedBuffer* CombinedBuffer::Allocate(const std::vector<VertexElement>& elements, size_t numVertices, size_t numIndices)
{
    unsigned key = VertexBuffer::CalculateAttributeMask(elements);
    auto it = buffers.find(key);
    if (it != buffers.end())
    {
        std::vector<WeakPtr<CombinedBuffer> >& keyBuffers = it->second;

        for (size_t i = 0; i < keyBuffers.size();)
        {
            CombinedBuffer* buffer = keyBuffers[i];
            // Clean up expired buffers
            if (!buffer)
            {
                keyBuffers.erase(keyBuffers.begin() + i);
                continue;
            }

            if (buffer->usedVertices + numVertices <= buffer->vertexBuffer->NumVertices() && buffer->usedIndices + numIndices <= buffer->indexBuffer->NumIndices())
                return buffer;

            ++i;
        }
    }

    // No existing buffer, make new
    LOGDEBUGF("Creating new combined buffer for attribute mask %d", key);
    CombinedBuffer* buffer = new CombinedBuffer(elements);

#ifdef _DEBUG
    if (it != buffers.end())
    {
        for (auto vIt = it->second.begin(); vIt != it->second.end(); ++vIt)
        {
            CombinedBuffer* prevBuffer = vIt->Get();
            LOGDEBUGF("Previous buffer use %d/%d %d/%d", prevBuffer->usedVertices, prevBuffer->vertexBuffer->NumVertices(), prevBuffer->usedIndices, prevBuffer->indexBuffer->NumIndices());
        }
    }
#endif

    buffers[key].push_back(buffer);
    return buffer;
}

ModelBone::ModelBone() :
    initialPosition(Vector3::ZERO),
    initialRotation(Quaternion::IDENTITY),
    initialScale(Vector3::ONE),
    offsetMatrix(Matrix3x4::IDENTITY),
    radius(0.0f),
    boundingBox(0.0f, 0.0f),
    parentIndex(0),
    active(true)
{
}

ModelBone::~ModelBone()
{
}

Model::Model()
{
}

Model::~Model()
{
}

void Model::RegisterObject()
{
    RegisterFactory<Model>();
}

bool Model::BeginLoad(Stream& source)
{
    ZoneScoped;

    /// \todo Develop own format for Turso3D
    if (source.ReadFileID() != "UMDL")
    {
        LOGERROR(source.Name() + " is not a valid model file");
        return false;
    }

    vbDescs.clear();
    ibDescs.clear();
    geomDescs.clear();

    size_t numVertexBuffers = source.Read<unsigned>();
    vbDescs.resize(numVertexBuffers);
    for (size_t i = 0; i < numVertexBuffers; ++i)
    {
        VertexBufferDesc& vbDesc = vbDescs[i];

        vbDesc.numVertices = source.Read<unsigned>();
        unsigned elementMask = source.Read<unsigned>();
        source.Read<unsigned>(); // morphRangeStart
        source.Read<unsigned>(); // morphRangeCount

        size_t vertexSize = 0;
        if (elementMask & 1)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 2)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_NORMAL));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 4)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_COLOR));
            vertexSize += 4;
        }
        if (elementMask & 8)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
            vertexSize += sizeof(Vector2);
        }
        if (elementMask & 16)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD, 1));
            vertexSize += sizeof(Vector2);
        }
        if (elementMask & 32)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 64)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD, 1));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 128)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_TANGENT));
            vertexSize += sizeof(Vector4);
        }
        if (elementMask & 256)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_VECTOR4, SEM_BLENDWEIGHTS));
            vertexSize += sizeof(Vector4);
        }
        if (elementMask & 512)
        {
            vbDesc.vertexElements.push_back(VertexElement(ELEM_UBYTE4, SEM_BLENDINDICES));
            vertexSize += 4;
        }

        vbDesc.vertexSize = vertexSize;
        vbDesc.vertexData = new unsigned char[vbDesc.numVertices * vertexSize];
        source.Read(&vbDesc.vertexData[0], vbDesc.numVertices * vertexSize);

        if (elementMask & 1)
        {
            vbDesc.cpuPositionData = new Vector3[vbDesc.numVertices];
            for (size_t j = 0; j < vbDesc.numVertices; ++j)
                vbDesc.cpuPositionData[j] = *reinterpret_cast<Vector3*>(vbDesc.vertexData + j * vertexSize);
        }
    }

    size_t numIndexBuffers = source.Read<unsigned>();
    ibDescs.resize(numIndexBuffers);
    for (size_t i = 0; i < numIndexBuffers; ++i)
    {
        IndexBufferDesc& ibDesc = ibDescs[i];
    
        ibDesc.numIndices = source.Read<unsigned>();
        ibDesc.indexSize = source.Read<unsigned>();
        ibDesc.indexData = new unsigned char[ibDesc.numIndices * ibDesc.indexSize];
        source.Read(&ibDesc.indexData[0], ibDesc.numIndices * ibDesc.indexSize);
    }

    size_t numGeometries = source.Read<unsigned>();

    geomDescs.resize(numGeometries);

    std::vector<std::vector<unsigned> > boneMappings;
    std::set<std::pair<unsigned, unsigned> > processedVertices;
    boneMappings.resize(numGeometries);

    for (size_t i = 0; i < numGeometries; ++i)
    {
        // Read bone mappings
        size_t boneMappingCount = source.Read<unsigned>();
        boneMappings[i].resize(boneMappingCount);
        if (boneMappingCount)
            source.Read(&boneMappings[i][0], boneMappingCount * sizeof(unsigned));

        size_t numLodLevels = source.Read<unsigned>();
        geomDescs[i].resize(numLodLevels);

        for (size_t j = 0; j < numLodLevels; ++j)
        {
            GeometryDesc& geomDesc = geomDescs[i][j];

            geomDesc.lodDistance = source.Read<float>();
            source.Read<unsigned>(); // Primitive type
            geomDesc.vbRef = source.Read<unsigned>();
            geomDesc.ibRef = source.Read<unsigned>();
            geomDesc.drawStart = source.Read<unsigned>();
            geomDesc.drawCount = source.Read<unsigned>();

            // Apply bone mappings to geometry
            if (boneMappingCount)
                ApplyBoneMappings(geomDesc, boneMappings[i], processedVertices);
        }
    }

    // Read (skip) morphs
    size_t numMorphs = source.Read<unsigned>();
    if (numMorphs)
    {
        LOGERROR("Models with vertex morphs are not supported for now");
        return false;
    }

    // Read skeleton
    size_t numBones = source.Read<unsigned>();
    bones.resize(numBones);
    for (size_t i = 0; i < numBones; ++i)
    {
        ModelBone& bone = bones[i];
        bone.name = source.Read<std::string>();
        bone.nameHash = StringHash(bone.name);
        bone.parentIndex = source.Read<unsigned>();
        bone.initialPosition = source.Read<Vector3>();
        bone.initialRotation = source.Read<Quaternion>();
        bone.initialScale = source.Read<Vector3>();
        bone.offsetMatrix = source.Read<Matrix3x4>();

        unsigned char boneCollisionType = source.Read<unsigned char>();
        if (boneCollisionType & 1)
        {
            bone.radius = source.Read<float>();
            if (bone.radius < BONE_SIZE_THRESHOLD * 0.5f)
                bone.active = false;
        }
        if (boneCollisionType & 2)
        {
            bone.boundingBox = source.Read<BoundingBox>();
            if (bone.boundingBox.Size().Length() < BONE_SIZE_THRESHOLD)
                bone.active = false;
        }
    }

    // Read bounding box
    boundingBox = source.Read<BoundingBox>();

    return true;
}

void Model::ApplyBoneMappings(const GeometryDesc& geomDesc, const std::vector<unsigned>& boneMappings, std::set<std::pair<unsigned, unsigned> >& processedVertices)
{
    ZoneScoped;

    size_t blendIndicesOffset = 0;
    bool blendIndicesFound = false;
    const VertexBufferDesc& vbDesc = vbDescs[geomDesc.vbRef];
    for (size_t i = 0; i < vbDesc.vertexElements.size(); ++i)
    {
        if (vbDesc.vertexElements[i].semantic == SEM_BLENDINDICES)
        {
            blendIndicesFound = true;
            break;
        }
        else
            blendIndicesOffset += VertexBuffer::VertexElementSize(vbDesc.vertexElements[i]);
    }

    if (!blendIndicesFound)
        return;

    unsigned char* blendIndicesData = vbDesc.vertexData + blendIndicesOffset;

    const IndexBufferDesc& ibDesc = ibDescs[geomDesc.ibRef];

    if (ibDesc.indexSize == sizeof(unsigned short))
    {
        unsigned short* indexData = (unsigned short*)ibDesc.indexData.Get();
        indexData += geomDesc.drawStart;
        
        unsigned drawCount = geomDesc.drawCount;
        while (drawCount--)
        {
            unsigned short vIndex = *indexData++;
            std::pair<unsigned, unsigned> vRef = std::make_pair(geomDesc.vbRef, vIndex);
            if (processedVertices.find(vRef) != processedVertices.end())
                continue;
            
            for (size_t i = 0; i < 4; ++i)
                blendIndicesData[vIndex * vbDesc.vertexSize + i] = (unsigned char)boneMappings[blendIndicesData[vIndex * vbDesc.vertexSize + i]];
            
            processedVertices.insert(vRef);
        }
    }
    else if (ibDesc.indexSize == sizeof(unsigned))
    {
        unsigned* indexData = (unsigned*)ibDesc.indexData.Get();
        indexData += geomDesc.drawStart;

        unsigned drawCount = geomDesc.drawCount;
        while (drawCount--)
        {
            unsigned vIndex = *indexData++;
            std::pair<unsigned, unsigned> vRef = std::make_pair(geomDesc.vbRef, vIndex);
            if (processedVertices.find(vRef) != processedVertices.end())
                continue;
            for (size_t i = 0; i < 4; ++i)
                blendIndicesData[vIndex * vbDesc.vertexSize + i] = (unsigned char)boneMappings[blendIndicesData[vIndex * vbDesc.vertexSize + i]];
            processedVertices.insert(vRef);
        }
    }
}

bool Model::EndLoad()
{
    ZoneScoped;

    bool hasWeights = false;
    bool hasSameIndexSize = true;
    size_t totalIndices = 0;

    for (size_t i = 0; i < ibDescs.size(); ++i)
    {
        totalIndices += ibDescs[i].numIndices;
        if (ibDescs[i].indexSize != ibDescs[0].indexSize)
            hasSameIndexSize = false;
    }

    for (auto it = vbDescs.begin(); it != vbDescs.end(); ++it)
    {
        for (auto vIt = it->vertexElements.begin(); vIt != it->vertexElements.end(); ++vIt)
        {
            if (vIt->semantic == SEM_BLENDWEIGHTS || vIt->semantic == SEM_BLENDINDICES)
            {
                hasWeights = true;
                break;
            }
        }
        if (hasWeights)
            break;
    }

    // Create the geometry structure early and fill the CPU-side data first. Fill actual vertex and index buffers later
    geometries.resize(geomDescs.size());
    for (size_t i = 0; i < geomDescs.size(); ++i)
    {
        geometries[i].resize(geomDescs[i].size());
        for (size_t j = 0; j < geomDescs[i].size(); ++j)
        {
            const GeometryDesc& geomDesc = geomDescs[i][j];
            SharedPtr<Geometry> geom(new Geometry());

            geom->lodDistance = geomDesc.lodDistance;
            geom->drawStart = geomDesc.drawStart;
            geom->drawCount = geomDesc.drawCount;
            geom->cpuPositionData = vbDescs[geomDesc.vbRef].cpuPositionData;
            geom->cpuIndexData = ibDescs[geomDesc.ibRef].indexData;
            geom->cpuIndexSize = ibDescs[geomDesc.ibRef].indexSize;
            geom->cpuDrawStart = geomDesc.drawStart;

            geometries[i][j] = geom;
        }
    }

    // Check if can use combined vertex / index buffers
    if (vbDescs.size() == 1 && vbDescs[0].numVertices < COMBINEDBUFFER_VERTICES && totalIndices < COMBINEDBUFFER_INDICES && hasSameIndexSize && !hasWeights)
    {
        combinedBuffer = CombinedBuffer::Allocate(vbDescs[0].vertexElements, vbDescs[0].numVertices, totalIndices);
        unsigned vertexStart = (unsigned)combinedBuffer->UsedVertices();

        for (size_t i = 0; i < ibDescs.size(); ++i)
        {
            IndexBufferDesc& ibDesc = ibDescs[i];

            if (ibDesc.indexSize == sizeof(unsigned short))
            {
                unsigned short* oldIndexData = (unsigned short*)&ibDesc.indexData[0];
                SharedArrayPtr<unsigned char> newIndices(new unsigned char[sizeof(unsigned) * ibDesc.numIndices]);
                unsigned* newIndexData = (unsigned*)newIndices.Get();
                for (size_t j = 0; j < ibDescs[i].numIndices; ++j)
                    newIndexData[j] = (unsigned)oldIndexData[j] + vertexStart;
                
                ibDesc.indexData = newIndices;
                ibDesc.indexSize = sizeof(unsigned);
            }
            else
            {
                unsigned* indexData = (unsigned*)&ibDesc.indexData[0];
                for (size_t j = 0; j < ibDescs[i].numIndices; ++j)
                    indexData[j] += vertexStart;
            }
        }

        std::vector<size_t> indexStarts;

        combinedBuffer->FillVertices(vbDescs[0].numVertices, vbDescs[0].vertexData);
        for (size_t i = 0; i < ibDescs.size(); ++i)
        {
            indexStarts.push_back(combinedBuffer->UsedIndices());
            combinedBuffer->FillIndices(ibDescs[i].numIndices, ibDescs[i].indexData);
        }

        for (size_t i = 0; i < geomDescs.size(); ++i)
        {
            for (size_t j = 0; j < geomDescs[i].size(); ++j)
            {
                const GeometryDesc& geomDesc = geomDescs[i][j];
                Geometry* geom = geometries[i][j];

                geom->vertexBuffer = combinedBuffer->GetVertexBuffer();
                geom->indexBuffer = combinedBuffer->GetIndexBuffer();
                geom->drawStart = geomDesc.drawStart + indexStarts[geomDesc.ibRef];
            }
        }

        vbDescs.clear();
        ibDescs.clear();
        geomDescs.clear();

        return true;
    }

    // If not, create individual buffers for this model and set them to the geometries
    std::vector<SharedPtr<VertexBuffer> > vbs;
    for (size_t i = 0; i < vbDescs.size(); ++i)
    {
        const VertexBufferDesc& vbDesc = vbDescs[i];
        SharedPtr<VertexBuffer> vb(new VertexBuffer());

        vb->Define(USAGE_DEFAULT, vbDesc.numVertices, vbDesc.vertexElements, vbDesc.vertexData);
        vbs.push_back(vb);
    }

    std::vector<SharedPtr<IndexBuffer> > ibs;
    for (size_t i = 0; i < ibDescs.size(); ++i)
    {
        const IndexBufferDesc& ibDesc = ibDescs[i];
        SharedPtr<IndexBuffer> ib(new IndexBuffer());

        ib->Define(USAGE_DEFAULT, ibDesc.numIndices, ibDesc.indexSize, ibDesc.indexData);
        ibs.push_back(ib);
    }

    geometries.resize(geomDescs.size());
    for (size_t i = 0; i < geomDescs.size(); ++i)
    {
        geometries[i].resize(geomDescs[i].size());
        for (size_t j = 0; j < geomDescs[i].size(); ++j)
        {
            const GeometryDesc& geomDesc = geomDescs[i][j];
            Geometry* geom = geometries[i][j];

            geom->vertexBuffer = vbs[geomDesc.vbRef];
            geom->indexBuffer = ibs[geomDesc.ibRef];
        }
    }

    vbDescs.clear();
    ibDescs.clear();
    geomDescs.clear();

    return true;
}

void Model::SetNumGeometries(size_t num)
{
    geometries.resize(num);
    // Ensure that each geometry has at least 1 LOD level
    for (size_t i = 0; i < geometries.size(); ++i)
    {
        if (!geometries[i].size())
            SetNumLodLevels(i, 1);
    }
}

void Model::SetNumLodLevels(size_t index, size_t num)
{
    if (index >= geometries.size())
    {
        LOGERROR("Out of bounds geometry index for setting number of LOD levels");
        return;
    }

    geometries[index].resize(num);
    // Ensure that a valid geometry object exists at each index
    for (auto it = geometries[index].begin(); it != geometries[index].end(); ++it)
    {
        if (it->IsNull())
            *it = new Geometry();
    }
}

void Model::SetLocalBoundingBox(const BoundingBox& box)
{
    boundingBox = box;
}

void Model::SetBones(const std::vector<ModelBone>& bones_)
{
    bones = bones_;
}

size_t Model::NumLodLevels(size_t index) const
{
    return index < geometries.size() ? geometries[index].size() : 0;
}

Geometry* Model::GetGeometry(size_t index, size_t lodLevel) const
{
    return (index < geometries.size() && lodLevel < geometries[index].size()) ? geometries[index][lodLevel] : nullptr;
}
