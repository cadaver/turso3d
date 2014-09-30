// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11Graphics.h"
#include "D3D11IndexBuffer.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

IndexBuffer::IndexBuffer() :
    buffer(0),
    numIndices(0),
    indexSize(0),
    dynamic(false)
{
}

IndexBuffer::~IndexBuffer()
{
    Release();
}

void IndexBuffer::Release()
{
    if (graphics && graphics->CurrentIndexBuffer() == this)
        graphics->SetIndexBuffer(0);
    
    if (buffer)
    {
        ID3D11Buffer* d3dBuffer = (ID3D11Buffer*)buffer;
        d3dBuffer->Release();
        buffer = 0;
    }

    shadowData.Reset();
    numIndices = 0;
    indexSize = 0;
}

bool IndexBuffer::Define(size_t numIndices_, size_t indexSize_, bool dynamic, bool shadow, const void* data)
{
    PROFILE(DefineIndexBuffer);

    Release();

    if (!numIndices_ || !indexSize_)
    {
        LOGERROR("Can not define vertex buffer with no vertices or no elements");
        return false;
    }
    if (!dynamic && !data)
    {
        LOGERROR("Non-dynamic buffer must define initial data, as it will be immutable after creation");
        return false;
    }
    if (indexSize_ != sizeof(unsigned) && indexSize_ != sizeof(unsigned short))
    {
        LOGERROR("Index size must be 2 or 4");
        return false;
    }

    numIndices = numIndices_;
    indexSize = indexSize_;

    if (shadow)
    {
        shadowData = new unsigned char[numIndices * indexSize];
        if (data)
            memcpy(shadowData.Get(), data, numIndices * indexSize);
    }

    if (graphics && graphics->IsInitialized())
    {
        D3D11_BUFFER_DESC bufferDesc;
        D3D11_SUBRESOURCE_DATA initialData;
        memset(&bufferDesc, 0, sizeof bufferDesc);
        memset(&initialData, 0, sizeof initialData);
        initialData.pSysMem = data;

        bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bufferDesc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        bufferDesc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
        bufferDesc.ByteWidth = numIndices * indexSize;

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateBuffer(&bufferDesc, data ? &initialData : 0, (ID3D11Buffer**)&buffer);

        if (!buffer)
        {
            LOGERROR("Failed to create index buffer");
            return false;
        }
        else
            LOGDEBUGF("Created index buffer size %u indexSize %u", (unsigned)numIndices, (unsigned)indexSize);
    }

    return true;
}

bool IndexBuffer::SetData(size_t firstIndex, size_t numIndices_, const void* data)
{
    PROFILE(UpdateIndexBuffer);

    if (!data)
    {
        LOGERROR("Null source data for updating index buffer");
        return false;
    }
    if (firstIndex + numIndices_ > numIndices)
    {
        LOGERROR("Out of bounds range for updating index buffer");
        return false;
    }
    if (buffer && !dynamic)
    {
        LOGERROR("Can not update non-dynamic index buffer");
        return false;
    }

    if (shadowData)
        memcpy(shadowData.Get() + firstIndex * indexSize, data, numIndices_ * indexSize);

    if (buffer)
    {
        D3D11_MAPPED_SUBRESOURCE mappedData;
        mappedData.pData = 0;

        ID3D11DeviceContext* d3dDeviceContext = (ID3D11DeviceContext*)graphics->DeviceContext();
        d3dDeviceContext->Map((ID3D11Buffer*)buffer, 0, numIndices_ == numIndices ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE, 0, &mappedData);
        if (mappedData.pData)
        {
            memcpy((unsigned char*)mappedData.pData + firstIndex * indexSize, data, numIndices_ * indexSize);
            d3dDeviceContext->Unmap((ID3D11Buffer*)buffer, 0);
        }
        else
        {
            LOGERROR("Failed to map index buffer for update");
            return false;
        }
    }

    return true;
}

}
