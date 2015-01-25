// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/VertexBuffer.h"
#include "../Scene/Node.h"
#include "GeometryNode.h"
#include "Material.h"
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

    vbDescs.Clear();
    ibDescs.Clear();
    geomDescs.Clear();

    size_t numVertexBuffers = source.Read<unsigned>();
    vbDescs.Resize(numVertexBuffers);
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
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_POSITION));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 2)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_NORMAL));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 4)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_UBYTE4, SEM_COLOR));
            vertexSize += 4;
        }
        if (elementMask & 8)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
            vertexSize += sizeof(Vector2);
        }
        if (elementMask & 16)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD, 1));
            vertexSize += sizeof(Vector2);
        }
        if (elementMask & 32)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 64)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR3, SEM_TEXCOORD, 1));
            vertexSize += sizeof(Vector3);
        }
        if (elementMask & 128)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_TANGENT));
            vertexSize += sizeof(Vector4);
        }
        if (elementMask & 256)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_VECTOR4, SEM_BLENDWEIGHT));
            vertexSize += sizeof(Vector4);
        }
        if (elementMask & 512)
        {
            vbDesc.vertexElements.Push(VertexElement(ELEM_UBYTE4, SEM_BLENDINDICES));
            vertexSize += 4;
        }

        vbDesc.vertexData = new unsigned char[vbDesc.numVertices * vertexSize];
        source.Read(&vbDesc.vertexData[0], vbDesc.numVertices * vertexSize);
    }

    size_t numIndexBuffers = source.Read<unsigned>();
    ibDescs.Resize(numIndexBuffers);
    for (size_t i = 0; i < numIndexBuffers; ++i)
    {
        IndexBufferDesc& ibDesc = ibDescs[i];
    
        ibDesc.numIndices = source.Read<unsigned>();
        ibDesc.indexSize = source.Read<unsigned>();
        ibDesc.indexData = new unsigned char[ibDesc.numIndices * ibDesc.indexSize];
        source.Read(&ibDesc.indexData[0], ibDesc.numIndices * ibDesc.indexSize);
    }

    size_t numGeometries = source.Read<unsigned>();

    geomDescs.Resize(numGeometries);
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
        geomDescs[i].Resize(numLodLevels);

        for (size_t j = 0; j < numLodLevels; ++j)
        {
            GeometryDesc& geomDesc = geomDescs[i][j];

            geomDesc.lodDistance = source.Read<float>();
            source.Read<unsigned>(); // Primitive type
            geomDesc.primitiveType = TRIANGLE_LIST; // Always assume triangle list for now
            geomDesc.vbRef = source.Read<unsigned>();
            geomDesc.ibRef = source.Read<unsigned>();
            geomDesc.drawStart = source.Read<unsigned>();
            geomDesc.drawCount = source.Read<unsigned>();
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
    Vector<SharedPtr<VertexBuffer> > vbs;
    for (size_t i = 0; i < vbDescs.Size(); ++i)
    {
        const VertexBufferDesc& vbDesc = vbDescs[i];
        SharedPtr<VertexBuffer> vb(new VertexBuffer());

        vb->Define(USAGE_IMMUTABLE, vbDesc.numVertices, vbDesc.vertexElements, true, vbDesc.vertexData.Get());
        vbs.Push(vb);
    }

    Vector<SharedPtr<IndexBuffer> > ibs;
    for (size_t i = 0; i < ibDescs.Size(); ++i)
    {
        const IndexBufferDesc& ibDesc = ibDescs[i];
        SharedPtr<IndexBuffer> ib(new IndexBuffer());

        ib->Define(USAGE_IMMUTABLE, ibDesc.numIndices, ibDesc.indexSize, true, ibDesc.indexData.Get());
        ibs.Push(ib);
    }

    // Set the buffers to each geometry
    geometries.Resize(geomDescs.Size());
    for (size_t i = 0; i < geomDescs.Size(); ++i)
    {
        geometries[i].Resize(geomDescs[i].Size());
        for (size_t j = 0; j < geomDescs[i].Size(); ++j)
        {
            const GeometryDesc& geomDesc = geomDescs[i][j];
            SharedPtr<Geometry> geom(new Geometry());

            geom->lodDistance = geomDesc.lodDistance;
            geom->primitiveType = geomDesc.primitiveType;
            geom->drawStart = geomDesc.drawStart;
            geom->drawCount = geomDesc.drawCount;
            
            if (geomDesc.vbRef < vbs.Size())
                geom->vertexBuffer = vbs[geomDesc.vbRef];
            else
                LOGERROR("Out of range vertex buffer reference in " + Name());

            if (geomDesc.ibRef < ibs.Size())
                geom->indexBuffer = ibs[geomDesc.ibRef];
            else
                LOGERROR("Out of range index buffer reference in " + Name());
            
            geometries[i][j] = geom;
        }
    }

    vbDescs.Clear();
    ibDescs.Clear();
    geomDescs.Clear();

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
