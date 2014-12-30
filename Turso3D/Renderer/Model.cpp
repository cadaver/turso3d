// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Scene/Node.h"
#include "GeometryNode.h"
#include "Model.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

Bone::Bone() :
    initialPosition(Vector3::ZERO),
    initialRotation(Quaternion::IDENTITY),
    initialScale(Vector3::ONE),
    offsetMatrix(Matrix3x4::IDENTITY),
    radius(0.0f),
    boundingBox(0.0f, 0.0f),
    parentIndex(0),
    animated(true)
{
}

Bone::~Bone()
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
    /// \todo Develop own format for Turso3D
    if (source.ReadFileID() != "UMDL")
    {
        LOGERROR(source.Name() + " is not a valid model file");
        return false;
    }

    geometries.Clear();

    size_t numVertexBuffers = source.Read<unsigned>();
    if (numVertexBuffers != 1)
    {
        LOGERROR("Only models with 1 vertex buffer are supported");
        return false;
    }

    vbDesc = new VertexBufferDesc();
    vbDesc->numVertices = source.Read<unsigned>();
    unsigned elementMask = source.Read<unsigned>();
    source.Read<unsigned>(); // morphRangeStart
    source.Read<unsigned>(); // morphRangeCount

    size_t vertexSize = 0;
    if (elementMask & 1)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_POSITION));
        vertexSize += sizeof(Vector3);
    }
    if (elementMask & 2)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_NORMAL));
        vertexSize += sizeof(Vector3);
    }
    if (elementMask & 4)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_UBYTE4, SEM_COLOR));
        vertexSize += 4;
    }
    if (elementMask & 8)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
        vertexSize += sizeof(Vector2);
    }
    if (elementMask & 16)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD, 1));
        vertexSize += sizeof(Vector2);
    }
    if (elementMask & 32)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD));
        vertexSize += sizeof(Vector3);
    }
    if (elementMask & 64)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD, 1));
        vertexSize += sizeof(Vector3);
    }
    if (elementMask & 128)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TANGENT));
        vertexSize += sizeof(Vector4);
    }
    if (elementMask & 256)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_BLENDWEIGHT));
        vertexSize += sizeof(Vector4);
    }
    if (elementMask & 512)
    {
        vbDesc->vertexElements.Push(VertexElement(ELEM_UBYTE4, SEM_BLENDINDICES));
        vertexSize += 4;
    }

    vbDesc->vertexData = new unsigned char[vbDesc->numVertices * vertexSize];
    source.Read(&vbDesc->vertexData[0], vbDesc->numVertices * vertexSize);

    unsigned numIndexBuffers = source.Read<unsigned>();
    if (numIndexBuffers != 1)
    {
        LOGERROR("Only models with 1 index buffer are supported");
        return false;
    }

    ibDesc = new IndexBufferDesc();
    ibDesc->numIndices = source.Read<unsigned>();
    ibDesc->indexSize = source.Read<unsigned>();
    ibDesc->indexData = new unsigned char[ibDesc->numIndices * ibDesc->indexSize];
    source.Read(&ibDesc->indexData[0], ibDesc->numIndices * ibDesc->indexSize);

    size_t numGeometries = source.Read<unsigned>();
    geometries.Resize(numGeometries);
    boneMappings.Resize(numGeometries);

    for (size_t i = 0; i < numGeometries; ++i)
    {
        // Read bone mappings
        size_t boneMappingCount = source.Read<unsigned>();
        boneMappings[i].Resize(boneMappingCount);
        /// \todo Should read as a batch
        for (size_t j = 0; j < boneMappingCount; ++j)
            boneMappings[i][j] = source.Read<unsigned>();

        size_t numLodLevels = source.Read<unsigned>();
        geometries[i].Resize(numLodLevels);

        for (unsigned j = 0; j < numLodLevels; ++j)
        {
            SharedPtr<Geometry> geom(new Geometry());
            geometries[i][j] = geom;
            geom->lodDistance = source.Read<float>();
            source.Read<unsigned>(); // Primitive type
            geom->primitiveType = TRIANGLE_LIST; // Always assume triangle list for now
            
            source.Read<unsigned>(); // vbRef
            source.Read<unsigned>(); // ibRef
            geom->drawStart = source.Read<unsigned>();
            geom->drawCount = source.Read<unsigned>();
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
    bones.Resize(numBones);
    for (size_t i = 0; i < numBones; ++i)
    {
        Bone& bone = bones[i];
        bone.name = source.Read<String>();
        bone.parentIndex = source.Read<unsigned>();
        bone.initialPosition = source.Read<Vector3>();
        bone.initialRotation = source.Read<Quaternion>();
        bone.initialScale = source.Read<Vector3>();
        bone.offsetMatrix = source.Read<Matrix3x4>();

        unsigned char boneCollisionType = source.Read<unsigned char>();
        if (boneCollisionType & 1)
            bone.radius = source.Read<float>();
        if (boneCollisionType & 2)
            bone.boundingBox = source.Read<BoundingBox>();

        if (bone.parentIndex == i)
            rootBoneIndex = i;
    }

    // Read bounding box
    boundingBox = source.Read<BoundingBox>();

    return true;
}

bool Model::EndLoad()
{
    if (!vbDesc || !ibDesc)
        return false;

    SharedPtr<VertexBuffer> vb(new VertexBuffer());
    SharedPtr<IndexBuffer> ib(new IndexBuffer());
    vb->Define(USAGE_IMMUTABLE, vbDesc->numVertices, vbDesc->vertexElements, true, vbDesc->vertexData.Get());
    ib->Define(USAGE_IMMUTABLE, ibDesc->numIndices, ibDesc->indexSize, true, ibDesc->indexData.Get());
    vbDesc.Reset();
    ibDesc.Reset();

    // Set the buffers to each geometry
    for (size_t i = 0; i < geometries.Size(); ++i)
    {
        for (size_t j = 0; j < geometries[i].Size(); ++j)
        {
            geometries[i][j]->vertexBuffer = vb;
            geometries[i][j]->indexBuffer = ib;
        }
    }

    return true;
}

void Model::SetNumGeometries(size_t num)
{
    geometries.Resize(num);
    // Ensure that each geometry has at least 1 LOD level
    for (size_t i = 0; i < geometries.Size(); ++i)
    {
        if (!geometries[i].Size())
            SetNumLodLevels(i, 1);
    }
}

void Model::SetNumLodLevels(size_t index, size_t num)
{
    if (index >= geometries.Size())
    {
        LOGERROR("Out of bounds geometry index for setting number of LOD levels");
        return;
    }

    geometries[index].Resize(num);
    // Ensure that a valid geometry object exists at each index
    for (auto it = geometries[index].Begin(); it != geometries[index].End(); ++it)
    {
        if (it->IsNull())
            *it = new Geometry();
    }
}

void Model::SetLocalBoundingBox(const BoundingBox& box)
{
    boundingBox = box;
}

void Model::SetBones(const Vector<Bone>& bones_, size_t rootBoneIndex_)
{
    bones = bones_;
    rootBoneIndex = rootBoneIndex_;
}

void Model::SetBoneMappings(const Vector<Vector<size_t> >& boneMappings_)
{
    boneMappings = boneMappings_;
}

size_t Model::NumLodLevels(size_t index) const
{
    return index < geometries.Size() ? geometries[index].Size() : 0;
}

Geometry* Model::GetGeometry(size_t index, size_t lodLevel) const
{
    return (index < geometries.Size() && lodLevel < geometries[index].Size()) ? geometries[index][lodLevel].Get() : nullptr;
}

}
