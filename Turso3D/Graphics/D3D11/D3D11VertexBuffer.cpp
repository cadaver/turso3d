// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Math/Matrix3x4.h"
#include "D3D11Graphics.h"
#include "D3D11VertexBuffer.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

const size_t VertexBuffer::elementSize[] =
{
    sizeof(int),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(unsigned),
    sizeof(Matrix3x4),
    sizeof(Matrix4)
};

const unsigned VertexBuffer::elementFormat[] = {
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R32G32B32A32_FLOAT, // Incorrect, but included to not cause out-of-range indexing
    DXGI_FORMAT_R32G32B32A32_FLOAT  //                          --||--
};

const char* VertexBuffer::elementSemantic[] = {
    "POSITION",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    nullptr
};

VertexBuffer::VertexBuffer() :
    buffer(nullptr),
    numVertices(0),
    vertexSize(0),
    elementHash(0),
    dynamic(false)
{
}

VertexBuffer::~VertexBuffer()
{
    Release();
}

void VertexBuffer::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (graphics->GetVertexBuffer(i) == this)
                graphics->SetVertexBuffer(i, 0);
        }
    }

    if (buffer)
    {
        ID3D11Buffer* d3dBuffer = (ID3D11Buffer*)buffer;
        d3dBuffer->Release();
        buffer = nullptr;
    }

    shadowData.Reset();
    elements.Clear();
    numVertices = 0;
    vertexSize = 0;
    elementHash = 0;
}

bool VertexBuffer::Define(size_t numVertices_, const Vector<VertexElement>& elements_, bool dynamic, bool shadow, const void* data)
{
    if (!numVertices_ || !elements_.Size())
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }

    return Define(numVertices_, elements_.Size(), &elements_[0], dynamic, shadow, data);
}

bool VertexBuffer::Define(size_t numVertices_, size_t numElements_, const VertexElement* elements_, bool dynamic, bool shadow, const void* data)
{
    PROFILE(DefineVertexBuffer);

    if (!numVertices_ || !numElements_ || !elements_)
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }

    if (!dynamic && !data)
    {
        LOGERROR("Non-dynamic buffer must define initial data, as it will be immutable after creation");
        return false;
    }

    for (size_t i = 0; i < numElements_; ++i)
    {
        if (elements_[i].type >= ELEM_MATRIX3X4)
        {
            LOGERROR("Matrix elements are not allowed in vertex buffers");
            return false;
        }
    }

    Release();

    numVertices = numVertices_;

    // Determine offset of elements and the vertex size & element hash
    vertexSize = 0;
    elementHash = 0;
    elements.Resize(numElements_);
    for (size_t i = 0; i < numElements_; ++i)
    {
        elements[i] = elements_[i];
        elements[i].offset = vertexSize;
        vertexSize += elementSize[elements[i].type];
        elementHash |= ElementHash(i, elements[i].semantic);
    }

    if (shadow)
    {
        shadowData = new unsigned char[numVertices * vertexSize];
        if (data)
            memcpy(shadowData.Get(), data, numVertices * vertexSize);
    }

    if (graphics && graphics->IsInitialized())
    {
        D3D11_BUFFER_DESC bufferDesc;
        D3D11_SUBRESOURCE_DATA initialData;
        memset(&bufferDesc, 0, sizeof bufferDesc);
        memset(&initialData, 0, sizeof initialData);
        initialData.pSysMem = data;

        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        bufferDesc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
        bufferDesc.ByteWidth = (unsigned)(numVertices * vertexSize);

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateBuffer(&bufferDesc, data ? &initialData : 0, (ID3D11Buffer**)&buffer);

        if (!buffer)
        {
            LOGERROR("Failed to create vertex buffer");
            return false;
        }
        else
            LOGDEBUGF("Created vertex buffer numVertices %u vertexSize %u", (unsigned)numVertices, (unsigned)vertexSize);
    }

    return true;
}

bool VertexBuffer::SetData(size_t firstVertex, size_t numVertices_, const void* data)
{
    PROFILE(UpdateVertexBuffer);

    if (!data)
    {
        LOGERROR("Null source data for updating vertex buffer");
        return false;
    }

    if (firstVertex + numVertices_ > numVertices)
    {
        LOGERROR("Out of bounds range for updating vertex buffer");
        return false;
    }

    if (buffer && !dynamic)
    {
        LOGERROR("Can not update non-dynamic vertex buffer");
        return false;
    }

    if (shadowData)
        memcpy(shadowData.Get() + firstVertex * vertexSize, data, numVertices_ * vertexSize);

    if (buffer)
    {
        D3D11_MAPPED_SUBRESOURCE mappedData;
        mappedData.pData = nullptr;

        ID3D11DeviceContext* d3dDeviceContext = (ID3D11DeviceContext*)graphics->DeviceContext();
        d3dDeviceContext->Map((ID3D11Buffer*)buffer, 0, numVertices_ == numVertices ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE, 0, &mappedData);
        if (mappedData.pData)
        {
            memcpy((unsigned char*)mappedData.pData + firstVertex * vertexSize, data, numVertices_ * vertexSize);
            d3dDeviceContext->Unmap((ID3D11Buffer*)buffer, 0);
        }
        else
        {
            LOGERROR("Failed to map vertex buffer for update");
            return false;
        }
    }

    return true;
}

}
