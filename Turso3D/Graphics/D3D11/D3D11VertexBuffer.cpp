// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11Graphics.h"
#include "D3D11VertexBuffer.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

const size_t VertexBuffer::elementSize[] =
{
    3 * sizeof(float), // Position
    3 * sizeof(float), // Normal
    4 * sizeof(unsigned char), // Color
    2 * sizeof(float), // Texcoord1
    2 * sizeof(float), // Texcoord2
    3 * sizeof(float), // Cubetexcoord1
    3 * sizeof(float), // Cubetexcoord2
    4 * sizeof(float), // Tangent
    4 * sizeof(float), // Blendweights
    4 * sizeof(unsigned char), // Blendindices
    4 * sizeof(float), // Instancematrix1
    4 * sizeof(float), // Instancematrix2
    4 * sizeof(float) // Instancematrix3
};

const char* VertexBuffer::elementSemantic[] = {
    "POSITION",
    "NORMAL",
    "COLOR",
    "TEXCOORD",
    "TEXCOORD",
    "CUBETEXCOORD",
    "CUBETEXCOORD",
    "TANGENT",
    "BLENDWEIGHTS",
    "BLENDINDICES",
    "INSTANCEMATRIX",
    "INSTANCEMATRIX",
    "INSTANCEMATRIX"
};

const unsigned VertexBuffer::elementSemanticIndex[] = {
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    1,
    2
};

const unsigned VertexBuffer::elementFormat[] = {
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT
};

VertexBuffer::VertexBuffer() :
    buffer(nullptr),
    numVertices(0),
    vertexSize(0),
    elementMask(0),
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
    numVertices = 0;
    vertexSize = 0;
    elementMask = 0;
}

bool VertexBuffer::Define(size_t numVertices_, unsigned elementMask_, bool dynamic, bool shadow, const void* data)
{
    PROFILE(DefineVertexBuffer);

    Release();

    if (!numVertices_ || !elementMask_)
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }

    if (!dynamic && !data)
    {
        LOGERROR("Non-dynamic buffer must define initial data, as it will be immutable after creation");
        return false;
    }

    numVertices = numVertices_;
    vertexSize = ComputeVertexSize(elementMask_);
    elementMask = elementMask_;

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

size_t VertexBuffer::ElementOffset(VertexElement element) const
{
    return ElementOffset(element, elementMask);
}

size_t VertexBuffer::ComputeVertexSize(unsigned elementMask)
{
    size_t vertexSize = 0;

    for (size_t i = 0; i < MAX_VERTEX_ELEMENTS; ++i)
    {
        if (elementMask & (1 << i))
            vertexSize += elementSize[i];
    }

    return vertexSize;
}

size_t VertexBuffer::ElementOffset(VertexElement element, unsigned elementMask)
{
    size_t offset = 0;

    for (size_t i = 0; i != (size_t)element; ++i)
    {
        if (elementMask & (1 << i))
            offset += elementSize[i];
    }

    return offset;
}

}
