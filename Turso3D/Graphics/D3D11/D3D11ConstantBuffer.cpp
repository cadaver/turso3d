// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Math/Color.h"
#include "../../Math/Matrix3.h"
#include "../../Math/Matrix3x4.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11Graphics.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

// Allow testing either Map/Unmap or UpdateSubResource
#define USE_UPDATESUBRESOURCE

namespace Turso3D
{

const size_t ConstantBuffer::elementSize[] =
{
    sizeof(int),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(Color),
    sizeof(Matrix3x4),
    sizeof(Matrix4)
};

ConstantBuffer::ConstantBuffer() :
    buffer(0),
    byteSize(0),
    dirty(false)
{
}

ConstantBuffer::~ConstantBuffer()
{
    Release();
}

void ConstantBuffer::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
        {
            for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            {
                if (graphics->CurrentConstantBuffer((ShaderStage)i, j) == this)
                    graphics->SetConstantBuffer((ShaderStage)i, j, 0);
            }
        }
    }
    
    if (buffer)
    {
        ID3D11Buffer* d3dBuffer = (ID3D11Buffer*)buffer;
        d3dBuffer->Release();
        buffer = 0;
    }

    shadowData.Reset();
}

bool ConstantBuffer::Define(const Vector<Constant>& srcConstants)
{
    return Define(srcConstants.Size(), srcConstants.Size() ? &srcConstants[0] : (const Constant*)0);
}

bool ConstantBuffer::Define(size_t numConstants, const Constant* srcConstants)
{
    PROFILE(DefineConstantBuffer);
    
    Release();
    
    if (!numConstants)
    {
        LOGERROR("Can not define constant buffer with no constants");
        return false;
    }
    
    constants.Clear();
    byteSize = 0;
    dirty = false;
    
    while (numConstants--)
    {
        /// \todo Pad variables automatically if they cross 16 byte borders
        Constant newConstant;
        newConstant.type = srcConstants->type;
        newConstant.name = srcConstants->name;
        newConstant.numElements = srcConstants->numElements;
        newConstant.elementSize = elementSize[newConstant.type];
        newConstant.offset = byteSize;
        constants.Push(newConstant);
        
        byteSize += newConstant.elementSize * newConstant.numElements;
        ++srcConstants;
    }

    // Align the final buffer size to a multiple of 16 bytes
    if (byteSize & 15)
        byteSize += 16 - (byteSize & 15);
    
    shadowData = new unsigned char[byteSize];

    if (graphics && graphics->IsInitialized())
    {
        D3D11_BUFFER_DESC bufferDesc;
        memset(&bufferDesc, 0, sizeof bufferDesc);
        
        bufferDesc.ByteWidth = (unsigned)byteSize;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
#ifdef USE_UPDATESUBRESOURCE
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
#else
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
#endif

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateBuffer(&bufferDesc, 0, (ID3D11Buffer**)&buffer);

        if (!buffer)
        {
            LOGERROR("Failed to create constant buffer");
            return false;
        }
        else
            LOGDEBUGF("Created constant buffer size %u", (unsigned)byteSize);
    }

    return true;
}

bool ConstantBuffer::SetConstant(size_t index, void* data, size_t numElements)
{
    if (index >= constants.Size())
        return false;
    
    const Constant& constant = constants[index];
    
    if (!numElements || numElements > constant.numElements)
        numElements = constant.numElements;
    
    memcpy(shadowData.Get() + constant.offset, data, numElements * constant.elementSize);
    dirty = true;
    return true;
}

bool ConstantBuffer::SetConstant(const String& name, void* data, size_t numElements)
{
    return SetConstant(name.CString(), data, numElements);
}

bool ConstantBuffer::SetConstant(const char* name, void* data, size_t numElements)
{
    for (size_t i = 0; i < constants.Size(); ++i)
    {
        if (constants[i].name == name)
            return SetConstant(i, data, numElements);
    }
    
    return false;
}

bool ConstantBuffer::Apply()
{
    if (!dirty || !buffer)
        return true;
    
    ID3D11DeviceContext* d3dDeviceContext = (ID3D11DeviceContext*)graphics->DeviceContext();
#ifdef USE_UPDATESUBRESOURCE
    d3dDeviceContext->UpdateSubresource((ID3D11Buffer*)buffer, 0, 0, shadowData.Get(), 0, 0);
#else
    D3D11_MAPPED_SUBRESOURCE mappedData;
    mappedData.pData = 0;

    d3dDeviceContext->Map((ID3D11Buffer*)buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
    if (mappedData.pData)
    {
        memcpy((unsigned char*)mappedData.pData, shadowData.Get(), byteSize);
        d3dDeviceContext->Unmap((ID3D11Buffer*)buffer, 0);
    }
    else
    {
        LOGERROR("Failed to map constant buffer for update");
        return false;
    }
#endif

    dirty = false;
    return true;
}

}