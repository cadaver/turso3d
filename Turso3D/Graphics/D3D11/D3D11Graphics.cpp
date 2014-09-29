// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Window/Window.h"
#include "../GPUObject.h"
#include "D3D11Graphics.h"
#include "D3D11IndexBuffer.h"
#include "D3D11ShaderVariation.h"
#include "D3D11VertexBuffer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cstdlib>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

const char* elementSemantic[] = {
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

UINT elementSemanticIndex[] = {
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

DXGI_FORMAT elementFormat[] = {
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

D3D11_PRIMITIVE_TOPOLOGY primitiveTopology[] = {
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
    D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
    D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
    D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP
};

/// %Graphics implementation. Holds OS-specific rendering API objects.
struct GraphicsImpl
{
    /// Construct.
    GraphicsImpl() :
        device(0),
        deviceContext(0),
        swapChain(0),
        backbufferView(0),
        depthTexture(0),
        depthStencilView(0)
    {
    }

    /// Graphics device.
    ID3D11Device* device;
    /// Immediate device context.
    ID3D11DeviceContext* deviceContext;
    /// Swap chain.
    IDXGISwapChain* swapChain;
    /// Backbuffer rendertarget view.
    ID3D11RenderTargetView* backbufferView;
    /// Default depth buffer texture.
    ID3D11Texture2D* depthTexture;
    /// Default depth stencil view.
    ID3D11DepthStencilView* depthStencilView;
};

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    fullscreen(false),
    inResize(false)
{
    RegisterSubsystem(this);
    impl = new GraphicsImpl();
    window = new Window();
    SubscribeToEvent(window->resizeEvent, &Graphics::HandleResize);
    ResetState();
}

Graphics::~Graphics()
{
    Close();
    RemoveSubsystem(this);
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool resizable)
{
    // Setting window size only required if window not open yet, otherwise the swapchain takes care of resizing
    if (!window->IsOpen())
    {
        if (!window->SetSize(width, height, resizable))
            return false;
    }

    // Create D3D11 device when setting mode for the first time
    if (!impl->device)
    {
        if (!CreateDevice())
            return false;
    }
    
    return UpdateSwapChain(width, height, fullscreen);
}

bool Graphics::SwitchFullscreen()
{
    return SetMode(backbufferSize.x, backbufferSize.y, !fullscreen, window->IsResizable());
}

void Graphics::Close()
{
    // Release all GPU objects
    for (Vector<GPUObject*>::Iterator it = gpuObjects.Begin(); it != gpuObjects.End(); ++it)
    {
        GPUObject* object = *it;
        object->Release();
    }

    for (HashMap<unsigned long long, void*>::Iterator it = inputLayouts.Begin(); it != inputLayouts.End(); ++it)
    {
        ID3D11InputLayout* d3dLayout = (ID3D11InputLayout*)it->second;
        d3dLayout->Release();
    }
    inputLayouts.Clear();

    if (impl->backbufferView)
    {
        impl->backbufferView->Release();
        impl->backbufferView = 0;
    }
    if (impl->depthStencilView)
    {
        impl->depthStencilView->Release();
        impl->depthStencilView = 0;
    }
    if (impl->depthTexture)
    {
        impl->depthTexture->Release();
        impl->depthTexture = 0;
    }
    if (impl->swapChain)
    {
        impl->swapChain->Release();
        impl->swapChain = 0;
    }
    if (impl->deviceContext)
    {
        impl->deviceContext->Release();
        impl->deviceContext = 0;
    }
    if (impl->device)
    {
        impl->device->Release();
        impl->device = 0;
    }
    
    window->Close();
    backbufferSize = IntVector2::ZERO;
    ResetState();
}

bool Graphics::IsInitialized() const
{
    return window != 0 && impl->device != 0;
}

Window* Graphics::RenderWindow() const
{
    return window;
}

void* Graphics::Device() const
{
    return impl->device;
}

void* Graphics::DeviceContext() const
{
    return impl->deviceContext;
}

VertexBuffer* Graphics::CurrentVertexBuffer(size_t index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers[index] : (VertexBuffer*)0;
}

void Graphics::AddGPUObject(GPUObject* object)
{
    if (object)
        gpuObjects.Push(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    // Note: implies a linear search, needs to be profiled whether becomes a problem with a large number of objects
    gpuObjects.Remove(object);
}

void Graphics::Clear(unsigned clearFlags, const Color& clearColor, float clearDepth, unsigned char clearStencil)
{
    if (!impl->device)
        return;

    if (clearFlags & CLEAR_COLOR)
        impl->deviceContext->ClearRenderTargetView(impl->backbufferView, clearColor.Data());
    if (clearFlags & (CLEAR_DEPTH|CLEAR_STENCIL))
    {
        UINT depthClearFlags = 0;
        if (clearFlags & CLEAR_DEPTH)
            depthClearFlags |= D3D11_CLEAR_DEPTH;
        if (clearFlags & CLEAR_STENCIL)
            depthClearFlags |= D3D11_CLEAR_STENCIL;
        impl->deviceContext->ClearDepthStencilView(impl->depthStencilView, depthClearFlags, clearDepth, clearStencil);
    }
}

void Graphics::Present()
{
    if (!impl->device)
        return;

    impl->swapChain->Present(0, 0);
}

void Graphics::SetVertexBuffer(size_t index, VertexBuffer* buffer)
{
    if (!impl->device)
        return;

    if (index < MAX_VERTEX_STREAMS && vertexBuffers[index] != buffer)
    {
        vertexBuffers[index] = buffer;
        if (buffer)
        {
            ID3D11Buffer* d3dBuffer = (ID3D11Buffer*)buffer->Buffer();
            UINT stride = buffer->VertexSize();
            UINT offset = 0;
            impl->deviceContext->IASetVertexBuffers(index, 1, &d3dBuffer, &stride, &offset);
        }
        else
        {
            ID3D11Buffer* d3dBuffer = 0;
            UINT zero = 0;
            impl->deviceContext->IASetVertexBuffers(index, 1, &d3dBuffer, &zero, &zero);
        }
        inputLayoutDirty = true;
    }
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (!impl->device)
        return;

    if (indexBuffer != buffer)
    {
        indexBuffer = buffer;
        if (buffer)
            impl->deviceContext->IASetIndexBuffer((ID3D11Buffer*)buffer->Buffer(), buffer->IndexSize() == sizeof(unsigned short) ?
                DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        else
            impl->deviceContext->IASetIndexBuffer(0, DXGI_FORMAT_UNKNOWN, 0);
    }
}

void Graphics::ResetVertexBuffers()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        SetVertexBuffer(i, 0);
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    if (!impl->device)
        return;

    if (vs != vertexShader)
    {
        if (vs && vs->Stage() == SHADER_VS)
        {
            if (!vs->IsCompiled())
                vs->Compile();
            impl->deviceContext->VSSetShader((ID3D11VertexShader*)vs->CompiledShader(), 0, 0);
        }
        else
            impl->deviceContext->VSSetShader(0, 0, 0);

        vertexShader = vs;
    }

    if (ps != pixelShader)
    {
        if (ps && ps->Stage() == SHADER_PS)
        {
            if (!ps->IsCompiled())
                ps->Compile();
            impl->deviceContext->PSSetShader((ID3D11PixelShader*)ps->CompiledShader(), 0, 0);
        }
        else
            impl->deviceContext->PSSetShader(0, 0, 0);

        pixelShader = ps;
    }
}

void Graphics::Draw(PrimitiveType type, size_t vertexStart, size_t vertexCount)
{
    if (!impl->device)
        return;

    PrepareDraw(type);
    impl->deviceContext->Draw((UINT)vertexCount, (UINT)vertexStart);
}

void Graphics::DrawIndexed(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    if (!impl->device)
        return;

    PrepareDraw(type);
    impl->deviceContext->DrawIndexed((UINT)indexCount, (UINT)indexStart, (UINT)vertexStart);
}

bool Graphics::CreateDevice()
{
    if (impl->device)
        return true; // Already exists

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    memset(&swapChainDesc, 0, sizeof swapChainDesc);

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = window->Width();
    swapChainDesc.BufferDesc.Height = window->Height();
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = (HWND)window->Handle();
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D11CreateDeviceAndSwapChain(
        0,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        0,
        0,
        0,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &impl->swapChain,
        &impl->device,
        0,
        &impl->deviceContext
    );

    if (!impl->device || !impl->deviceContext || !impl->swapChain)
    {
        LOGERROR("Failed to create D3D11 device");
        return false;
    }

    // Disable automatic fullscreen switching
    IDXGIFactory* factory;
    impl->swapChain->GetParent(__uuidof(IDXGIFactory), (void**)&factory);
    if (factory)
    {
        factory->MakeWindowAssociation((HWND)window->Handle(), DXGI_MWA_NO_ALT_ENTER);
        factory->Release();
    }

    return true;
}

bool Graphics::UpdateSwapChain(int width, int height, bool fullscreen_)
{
    if (inResize)
        return true;

    inResize = true;
    bool success = true;

    ID3D11RenderTargetView* nullView = 0;
    impl->deviceContext->OMSetRenderTargets(1, &nullView, (ID3D11DepthStencilView*)0);
    if (impl->backbufferView)
    {
        impl->backbufferView->Release();
        impl->backbufferView = 0;
    }
    if (impl->depthStencilView)
    {
        impl->depthStencilView->Release();
        impl->depthStencilView = 0;
    }
    if (impl->depthTexture)
    {
        impl->depthTexture->Release();
        impl->depthTexture = 0;
    }

    DXGI_MODE_DESC modeDesc;
    memset(&modeDesc, 0, sizeof modeDesc);
    modeDesc.Format = DXGI_FORMAT_UNKNOWN;
    modeDesc.Width = width;
    modeDesc.Height = height;

    // For fastest and most glitch-free switching, switch to full screen last and back to windowed first
    /// \todo Enumerate fullscreen modes and ensure the requested width/height are allowed
    if (fullscreen_)
    {
        impl->swapChain->ResizeTarget(&modeDesc);
        impl->swapChain->SetFullscreenState(fullscreen_, 0);
    }
    else
    {
        impl->swapChain->SetFullscreenState(fullscreen_, 0);
        impl->swapChain->ResizeTarget(&modeDesc);
    }

    impl->swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

    // Read back the actual width/height that has taken effect
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    memset(&swapChainDesc, 0, sizeof swapChainDesc);
    impl->swapChain->GetDesc(&swapChainDesc);
    width = swapChainDesc.BufferDesc.Width;
    height = swapChainDesc.BufferDesc.Height;

    // Get the backbuffer
    ID3D11Texture2D* backbufferTexture;
    impl->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTexture);
    if (backbufferTexture)
    {
        impl->device->CreateRenderTargetView(backbufferTexture, 0, &impl->backbufferView);
        backbufferTexture->Release();
    }
    else
    {
        LOGERROR("Failed to get backbuffer texture");
        success = false;
    }

    // Create default depth stencil
    D3D11_TEXTURE2D_DESC depthDesc;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;
    impl->device->CreateTexture2D(&depthDesc, 0, &impl->depthTexture);
    if (impl->depthTexture)
        impl->device->CreateDepthStencilView(impl->depthTexture, 0, &impl->depthStencilView);
    else
    {
        LOGERROR("Failed to create depth texture");
        success = false;
    }

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    impl->deviceContext->OMSetRenderTargets(1, &impl->backbufferView, impl->depthStencilView);
    impl->deviceContext->RSSetViewports(1, &vp);

    // Update internally held backbuffer size and fullscreen state
    backbufferSize.x = width;
    backbufferSize.y = height;
    fullscreen = fullscreen_;

    inResize = false;
    return success;
}

void Graphics::HandleResize(WindowResizeEvent& /*event*/)
{
    // Handle windowed mode resize
    if (impl->swapChain && !fullscreen && (window->Width() != backbufferSize.x || window->Height() != backbufferSize.y))
        UpdateSwapChain(window->Width(), window->Height(), false);
}

void Graphics::PrepareDraw(PrimitiveType type)
{
    if (primitiveType != type)
    {
        impl->deviceContext->IASetPrimitiveTopology(primitiveTopology[type]);
        primitiveType = type;
    }

    if (inputLayoutDirty)
    {
        inputLayoutDirty = false;
        unsigned long long totalMask = 0;
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (vertexBuffers[i])
                totalMask |= (unsigned long long)vertexBuffers[i]->ElementMask() << (i * MAX_VERTEX_ELEMENTS);
        }

        if (!totalMask || inputLayout == totalMask)
            return;

        // Check if layout already exists
        HashMap<unsigned long long, void*>::Iterator it = inputLayouts.Find(totalMask);
        if (it != inputLayouts.End())
        {
            impl->deviceContext->IASetInputLayout((ID3D11InputLayout*)it->second);
            inputLayout = totalMask;
            return;
        }

        // Not found, create new
        size_t currentOffset = 0;
        Vector<D3D11_INPUT_ELEMENT_DESC> elementDescs;

        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (vertexBuffers[i])
            {
                unsigned elementMask = vertexBuffers[i]->ElementMask();

                for (size_t j = 0; j < MAX_VERTEX_ELEMENTS; ++j)
                {
                    if (elementMask & (1 << j))
                    {
                        D3D11_INPUT_ELEMENT_DESC newDesc;
                        newDesc.SemanticName = elementSemantic[j];
                        newDesc.SemanticIndex = elementSemanticIndex[j];
                        newDesc.Format = elementFormat[j];
                        newDesc.InputSlot = i;
                        newDesc.AlignedByteOffset = currentOffset;
                        newDesc.InputSlotClass = j < ELEMENT_INSTANCEMATRIX1 ? D3D11_INPUT_PER_VERTEX_DATA :  D3D11_INPUT_PER_INSTANCE_DATA;
                        newDesc.InstanceDataStepRate = j < ELEMENT_INSTANCEMATRIX1 ? 0 : 1;
                        elementDescs.Push(newDesc);

                        currentOffset += VertexBuffer::elementSize[j];
                    }
                }
            }
        }

        ID3D11InputLayout* newLayout = 0;
        ID3DBlob* d3dBlob = (ID3DBlob*)vertexShader->CompiledBlob();
        impl->device->CreateInputLayout(&elementDescs[0], elementDescs.Size(), d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), &newLayout);
        if (newLayout)
        {
            inputLayouts[totalMask] = newLayout;
            impl->deviceContext->IASetInputLayout(newLayout);
            inputLayout = totalMask;
        }
        else
            LOGERROR("Failed to create input layout");
    }
}

void Graphics::ResetState()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers[i] = 0;
    indexBuffer = 0;
    vertexShader = 0;
    pixelShader = 0;
    inputLayout = 0;
    inputLayoutDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
}

}
