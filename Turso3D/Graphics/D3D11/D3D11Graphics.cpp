// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Window/Window.h"
#include "../GPUObject.h"
#include "../Shader.h"
#include "D3D11BlendState.h"
#include "D3D11DepthState.h"
#include "D3D11Graphics.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11IndexBuffer.h"
#include "D3D11RasterizerState.h"
#include "D3D11ShaderVariation.h"
#include "D3D11Texture.h"
#include "D3D11VertexBuffer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <cstdlib>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

static const DXGI_FORMAT d3dElementFormats[] = {
    DXGI_FORMAT_R32_SINT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R32G32B32A32_FLOAT, // Incorrect, but included to not cause out-of-range indexing
    DXGI_FORMAT_R32G32B32A32_FLOAT  //                          --||--
};

/// %Graphics implementation. Holds OS-specific rendering API objects.
struct GraphicsImpl
{
    /// Construct.
    GraphicsImpl() :
        device(nullptr),
        deviceContext(nullptr),
        swapChain(nullptr),
        defaultRenderTargetView(nullptr),
        defaultDepthTexture(nullptr),
        defaultDepthStencilView(nullptr),
        depthStencilView(nullptr)
    {
        for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
            renderTargetViews[i] = nullptr;
    }

    /// Graphics device.
    ID3D11Device* device;
    /// Immediate device context.
    ID3D11DeviceContext* deviceContext;
    /// Swap chain.
    IDXGISwapChain* swapChain;
    /// Default (backbuffer) rendertarget view.
    ID3D11RenderTargetView* defaultRenderTargetView;
    /// Default depth-stencil texture.
    ID3D11Texture2D* defaultDepthTexture;
    /// Default depth-stencil view.
    ID3D11DepthStencilView* defaultDepthStencilView;
    /// Current blend state object.
    ID3D11BlendState* blendState;
    /// Current depth stencil state object.
    ID3D11DepthStencilState* depthStencilState;
    /// Current rasterizer state object.
    ID3D11RasterizerState* rasterizerState;
    /// Current shader resource views.
    ID3D11ShaderResourceView* resourceViews[MAX_TEXTURE_UNITS];
    /// Current sampler states.
    ID3D11SamplerState* samplers[MAX_TEXTURE_UNITS];
    /// Current color rendertarget views.
    ID3D11RenderTargetView* renderTargetViews[MAX_RENDERTARGETS];
    /// Current depth-stencil view.
    ID3D11DepthStencilView* depthStencilView;
};

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    renderTargetSize(IntVector2::ZERO),
    vsync(false)
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
    if (!window->SetSize(width, height, fullscreen, resizable))
        return false;

    // Create D3D11 device when setting mode for the first time
    if (!impl->device)
    {
        if (!CreateD3DDevice())
            return false;
        // Swap chain needs to be updated manually for the first time, otherwise window resize event takes care of it
        UpdateSwapChain(window->Width(), window->Height());
    }

    return true;
}

bool Graphics::SetFullscreen(bool enable)
{
    if (!IsInitialized())
        return false;
    else
        return SetMode(backbufferSize.x, backbufferSize.y, enable, window->IsResizable());
}

void Graphics::SetVSync(bool enable)
{
    vsync = enable;
}

void Graphics::Close()
{
    // Release all GPU objects
    for (auto it = gpuObjects.Begin(); it != gpuObjects.End(); ++it)
    {
        GPUObject* object = *it;
        object->Release();
    }

    for (auto it = inputLayouts.Begin(); it != inputLayouts.End(); ++it)
    {
        ID3D11InputLayout* d3dLayout = (ID3D11InputLayout*)it->second;
        d3dLayout->Release();
    }
    inputLayouts.Clear();

    if (impl->deviceContext)
    {
        ID3D11RenderTargetView* nullView = nullptr;
        impl->deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
    }
    if (impl->defaultRenderTargetView)
    {
        impl->defaultRenderTargetView->Release();
        impl->defaultRenderTargetView = nullptr;
    }
    if (impl->defaultDepthStencilView)
    {
        impl->defaultDepthStencilView->Release();
        impl->defaultDepthStencilView = nullptr;
    }
    if (impl->defaultDepthTexture)
    {
        impl->defaultDepthTexture->Release();
        impl->defaultDepthTexture = nullptr;
    }
    if (impl->swapChain)
    {
        impl->swapChain->Release();
        impl->swapChain = nullptr;
    }
    if (impl->deviceContext)
    {
        impl->deviceContext->Release();
        impl->deviceContext = nullptr;
    }
    if (impl->device)
    {
        impl->device->Release();
        impl->device = nullptr;
    }
    
    window->Close();
    backbufferSize = IntVector2::ZERO;
    ResetState();
}

void Graphics::Present()
{
    impl->swapChain->Present(vsync ? 1 : 0, 0);
}

void Graphics::SetRenderTarget(Texture* renderTarget_, Texture* depthStencil_)
{
    renderTargetVector.Resize(1);
    renderTargetVector[0] = renderTarget_;
    SetRenderTargets(renderTargetVector, depthStencil_);
}

void Graphics::SetRenderTargets(const Vector<Texture*>& renderTargets_, Texture* depthStencil_)
{
    if (renderTargets_.IsEmpty())
        return;

    for (size_t i = 0; i < MAX_RENDERTARGETS && i < renderTargets_.Size(); ++i)
    {
        renderTargets[i] = (renderTargets_[i] && renderTargets_[i]->IsRenderTarget()) ? renderTargets_[i] : nullptr;
        impl->renderTargetViews[i] = renderTargets[i] ? (ID3D11RenderTargetView*)renderTargets_[i]->D3DRenderTargetView() :
            impl->defaultRenderTargetView;
    }

    for (size_t i = renderTargets_.Size(); i < MAX_RENDERTARGETS; ++i)
    {
        renderTargets[i] = nullptr;
        impl->renderTargetViews[i] = nullptr;
    }

    depthStencil = (depthStencil_ && depthStencil_->IsDepthStencil()) ? depthStencil_ : nullptr;
    impl->depthStencilView = depthStencil ? (ID3D11DepthStencilView*)depthStencil->D3DRenderTargetView() :
        impl->defaultDepthStencilView;

    if (renderTargets[0])
        renderTargetSize = IntVector2(renderTargets[0]->Width(), renderTargets[0]->Height());
    else if (depthStencil)
        renderTargetSize = IntVector2(depthStencil->Width(), depthStencil->Height());
    else
        renderTargetSize = backbufferSize;

    impl->deviceContext->OMSetRenderTargets(Min((int)renderTargets_.Size(), (int)MAX_RENDERTARGETS), impl->renderTargetViews, impl->depthStencilView);
}

void Graphics::SetViewport(const IntRect& viewport_)
{
    /// \todo Implement a member function in IntRect for clipping
    viewport.left = Clamp(viewport_.left, 0, renderTargetSize.x - 1);
    viewport.top = Clamp(viewport_.top, 0, renderTargetSize.y - 1);
    viewport.right = Clamp(viewport_.right, viewport.left + 1, renderTargetSize.x);
    viewport.bottom = Clamp(viewport_.bottom, viewport.top + 1, renderTargetSize.y);

    D3D11_VIEWPORT d3dViewport;
    d3dViewport.TopLeftX = (float)viewport.left;
    d3dViewport.TopLeftY = (float)viewport.top;
    d3dViewport.Width = (float)(viewport.right - viewport.left);
    d3dViewport.Height = (float)(viewport.bottom - viewport.top);
    d3dViewport.MinDepth = 0.0f;
    d3dViewport.MaxDepth = 1.0f;

    impl->deviceContext->RSSetViewports(1, &d3dViewport);
}

void Graphics::SetVertexBuffer(size_t index, VertexBuffer* buffer)
{
    if (index < MAX_VERTEX_STREAMS && buffer != vertexBuffers[index])
    {
        vertexBuffers[index] = buffer;
        ID3D11Buffer* d3dBuffer = buffer ? (ID3D11Buffer*)buffer->D3DBuffer() : nullptr;
        unsigned stride = buffer ? (unsigned)buffer->VertexSize() : 0;
        unsigned offset = 0;
        impl->deviceContext->IASetVertexBuffers((unsigned)index, 1, &d3dBuffer, &stride, &offset);
        inputLayoutDirty = true;
    }
}

void Graphics::SetConstantBuffer(ShaderStage stage, size_t index, ConstantBuffer* buffer)
{
    if (stage < MAX_SHADER_STAGES && index < MAX_CONSTANT_BUFFERS && buffer != constantBuffers[stage][index])
    {
        constantBuffers[stage][index] = buffer;
        ID3D11Buffer* d3dBuffer = buffer ? (ID3D11Buffer*)buffer->D3DBuffer() : nullptr;

        switch (stage)
        {
        case SHADER_VS:
            impl->deviceContext->VSSetConstantBuffers((unsigned)index, 1, &d3dBuffer);
            break;
            
        case SHADER_PS:
            impl->deviceContext->PSSetConstantBuffers((unsigned)index, 1, &d3dBuffer);
            break;
            
        default:
            break;
        }
    }
}

void Graphics::SetTexture(size_t index, Texture* texture)
{
    if (index < MAX_TEXTURE_UNITS)
    {
        textures[index] = texture;
        ID3D11ShaderResourceView* d3dResourceView = texture ? (ID3D11ShaderResourceView*)texture->D3DResourceView() :
            nullptr;
        ID3D11SamplerState* d3dSampler = texture ? (ID3D11SamplerState*)texture->D3DSampler() : nullptr;
        // Note: now both VS & PS resource views are set at the same time, to mimic OpenGL conventions
        if (d3dResourceView != impl->resourceViews[index])
        {
            impl->resourceViews[index] = d3dResourceView;
            impl->deviceContext->VSSetShaderResources((unsigned)index, 1, &d3dResourceView);
            impl->deviceContext->PSSetShaderResources((unsigned)index, 1, &d3dResourceView);
        }
        if (d3dSampler != impl->samplers[index])
        {
            impl->samplers[index] = d3dSampler;
            impl->deviceContext->VSSetSamplers((unsigned)index, 1, &d3dSampler);
            impl->deviceContext->PSSetSamplers((unsigned)index, 1, &d3dSampler);
        }
    }
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (buffer != indexBuffer)
    {
        indexBuffer = buffer;
        if (buffer)
            impl->deviceContext->IASetIndexBuffer((ID3D11Buffer*)buffer->D3DBuffer(), buffer->IndexSize() ==
                sizeof(unsigned short) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        else
            impl->deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    }
}

void Graphics::SetShaders(ShaderVariation* vs, ShaderVariation* ps)
{
    if (vs != vertexShader)
    {
        if (vs && vs->Stage() == SHADER_VS)
        {
            if (!vs->IsCompiled())
                vs->Compile();
            impl->deviceContext->VSSetShader((ID3D11VertexShader*)vs->ShaderObject(), nullptr, 0);
        }
        else
            impl->deviceContext->VSSetShader(nullptr, nullptr, 0);

        vertexShader = vs;
    }

    if (ps != pixelShader)
    {
        if (ps && ps->Stage() == SHADER_PS)
        {
            if (!ps->IsCompiled())
                ps->Compile();
            impl->deviceContext->PSSetShader((ID3D11PixelShader*)ps->ShaderObject(), nullptr, 0);
        }
        else
            impl->deviceContext->PSSetShader(nullptr, nullptr, 0);

        pixelShader = ps;
    }
}

void Graphics::SetBlendState(BlendState* state)
{
    if (state != blendState)
    {
        ID3D11BlendState* d3dBlendState = state ? (ID3D11BlendState*)state->D3DState() : nullptr;
        if (d3dBlendState != impl->blendState)
        {
            impl->deviceContext->OMSetBlendState(d3dBlendState, 0, 0xffffffff);
            impl->blendState = d3dBlendState;
        }
        blendState = state;
    }
}

void Graphics::SetDepthState(DepthState* state, unsigned char stencilRef_)
{
    if (state != depthState || stencilRef_ != stencilRef)
    {
        ID3D11DepthStencilState* d3dDepthStencilState = state ? (ID3D11DepthStencilState*)state->D3DState() : nullptr;
        if (d3dDepthStencilState != impl->depthStencilState || stencilRef_ != stencilRef)
        {
            impl->deviceContext->OMSetDepthStencilState(d3dDepthStencilState, stencilRef_);
            impl->depthStencilState = d3dDepthStencilState;
            stencilRef = stencilRef_;
        }
        depthState = state;
    }
}

void Graphics::SetRasterizerState(RasterizerState* state)
{
    if (state != rasterizerState)
    {
        ID3D11RasterizerState* d3dRasterizerState = state ? (ID3D11RasterizerState*)state->D3DState() : nullptr;
        if (d3dRasterizerState != impl->rasterizerState)
        {
            impl->deviceContext->RSSetState(d3dRasterizerState);
            impl->rasterizerState = d3dRasterizerState;
        }
        rasterizerState = state;
    }
}

void Graphics::SetScissorRect(const IntRect& scissorRect_)
{
    if (scissorRect_ != scissorRect)
    {
        /// \todo Implement a member function in IntRect for clipping
        scissorRect.left = Clamp(scissorRect_.left, 0, renderTargetSize.x - 1);
        scissorRect.top = Clamp(scissorRect_.top, 0, renderTargetSize.y - 1);
        scissorRect.right = Clamp(scissorRect_.right, scissorRect.left + 1, renderTargetSize.x);
        scissorRect.bottom = Clamp(scissorRect_.bottom, scissorRect.top + 1, renderTargetSize.y);

        D3D11_RECT d3dRect;
        d3dRect.left = scissorRect.left;
        d3dRect.top = scissorRect.top;
        d3dRect.right = scissorRect.right;
        d3dRect.bottom = scissorRect.bottom;
        impl->deviceContext->RSSetScissorRects(1, &d3dRect);
    }
}

void Graphics::ResetRenderTargets()
{
    SetRenderTarget(nullptr, nullptr);
}

void Graphics::ResetViewport()
{
    SetViewport(IntRect(0, 0, renderTargetSize.x, renderTargetSize.y));
}

void Graphics::ResetVertexBuffers()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        SetVertexBuffer(i, nullptr);
}

void Graphics::ResetConstantBuffers()
{
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; i < MAX_CONSTANT_BUFFERS; ++j)
            SetConstantBuffer((ShaderStage)i, j, nullptr);
    }
}

void Graphics::ResetTextures()
{
    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
        SetTexture(i, nullptr);
}

void Graphics::Clear(unsigned clearFlags, const Color& clearColor, float clearDepth, unsigned char clearStencil)
{
    if ((clearFlags & CLEAR_COLOR) && impl->renderTargetViews[0])
        impl->deviceContext->ClearRenderTargetView(impl->renderTargetViews[0], clearColor.Data());
    
    if ((clearFlags & (CLEAR_DEPTH | CLEAR_STENCIL)) && impl->depthStencilView)
    {
        unsigned depthClearFlags = 0;
        if (clearFlags & CLEAR_DEPTH)
            depthClearFlags |= D3D11_CLEAR_DEPTH;
        if (clearFlags & CLEAR_STENCIL)
            depthClearFlags |= D3D11_CLEAR_STENCIL;
        impl->deviceContext->ClearDepthStencilView(impl->depthStencilView, depthClearFlags, clearDepth, clearStencil);
    }
}

void Graphics::Draw(PrimitiveType type, size_t vertexStart, size_t vertexCount)
{
    PrepareDraw(type);
    impl->deviceContext->Draw((unsigned)vertexCount, (unsigned)vertexStart);
}

void Graphics::Draw(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    PrepareDraw(type);
    impl->deviceContext->DrawIndexed((unsigned)indexCount, (unsigned)indexStart, (unsigned)vertexStart);
}

void Graphics::DrawInstanced(PrimitiveType type, size_t vertexStart, size_t vertexCount, size_t instanceStart, size_t instanceCount)
{
    PrepareDraw(type);
    impl->deviceContext->DrawInstanced((unsigned)vertexCount, (unsigned)instanceCount, (unsigned)vertexStart, (unsigned)instanceStart);
}

void Graphics::DrawInstanced(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart, size_t instanceStart, size_t instanceCount)
{
    PrepareDraw(type);
    impl->deviceContext->DrawIndexedInstanced((unsigned)indexCount, (unsigned)instanceCount, (unsigned)indexStart, (unsigned)vertexStart, (unsigned)instanceStart);
}

bool Graphics::IsInitialized() const
{
    return window->IsOpen() && impl->device != nullptr;
}

bool Graphics::IsFullscreen() const
{
    return window->IsFullscreen();
}

bool Graphics::IsResizable() const
{
    return window->IsResizable();
}

Window* Graphics::RenderWindow() const
{
    return window;
}

Texture* Graphics::RenderTarget(size_t index) const
{
    return index < MAX_RENDERTARGETS ? renderTargets[index] : nullptr;
}

VertexBuffer* Graphics::GetVertexBuffer(size_t index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers[index] : nullptr;
}

ConstantBuffer* Graphics::GetConstantBuffer(ShaderStage stage, size_t index) const
{
    return (stage < MAX_SHADER_STAGES && index < MAX_CONSTANT_BUFFERS) ? constantBuffers[stage][index] : nullptr;
}

Texture* Graphics::GetTexture(size_t index) const
{
    return (index < MAX_TEXTURE_UNITS) ? textures[index] : nullptr;
}

void Graphics::AddGPUObject(GPUObject* object)
{
    if (object)
        gpuObjects.Push(object);
}

void Graphics::RemoveGPUObject(GPUObject* object)
{
    /// \todo Requires a linear search, needs to be profiled whether becomes a problem with a large number of objects
    gpuObjects.Remove(object);
}

void* Graphics::D3DDevice() const
{
    return impl->device;
}

void* Graphics::D3DDeviceContext() const
{
    return impl->deviceContext;
}

bool Graphics::CreateD3DDevice()
{
    if (impl->device)
        return true; // Already exists

    // Create device first
    D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        0,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &impl->device,
        nullptr,
        &impl->deviceContext
    );

    if (!impl->device || !impl->deviceContext)
    {
        LOGERROR("Failed to create D3D11 device");
        return false;
    }

    // Create swap chain
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

    IDXGIDevice* dxgiDevice = nullptr;
    impl->device->QueryInterface(IID_IDXGIDevice, (void **)&dxgiDevice);
    IDXGIAdapter* dxgiAdapter = nullptr;
    dxgiDevice->GetParent(IID_IDXGIAdapter, (void **)&dxgiAdapter);
    IDXGIFactory* dxgiFactory = nullptr;
    dxgiAdapter->GetParent(IID_IDXGIFactory, (void **)&dxgiFactory);
    dxgiFactory->CreateSwapChain(impl->device, &swapChainDesc, &impl->swapChain);
    // After creating the swap chain, disable automatic Alt-Enter fullscreen/windowed switching
    // (the application will switch manually if it wants to)
    dxgiFactory->MakeWindowAssociation((HWND)window->Handle(), DXGI_MWA_NO_ALT_ENTER);

    dxgiFactory->Release();
    dxgiAdapter->Release();
    dxgiDevice->Release();

    if (!impl->swapChain)
    {
        LOGERROR("Failed to create D3D11 swap chain");
        return false;
    }

    return true;
}

bool Graphics::UpdateSwapChain(int width, int height)
{
    bool success = true;

    ID3D11RenderTargetView* nullView = nullptr;
    impl->deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
    if (impl->defaultRenderTargetView)
    {
        impl->defaultRenderTargetView->Release();
        impl->defaultRenderTargetView = nullptr;
    }
    if (impl->defaultDepthStencilView)
    {
        impl->defaultDepthStencilView->Release();
        impl->defaultDepthStencilView = nullptr;
    }
    if (impl->defaultDepthTexture)
    {
        impl->defaultDepthTexture->Release();
        impl->defaultDepthTexture = nullptr;
    }

    impl->swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

    // Create default rendertarget view representing the backbuffer
    ID3D11Texture2D* backbufferTexture;
    impl->swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backbufferTexture);
    if (backbufferTexture)
    {
        impl->device->CreateRenderTargetView(backbufferTexture, 0, &impl->defaultRenderTargetView);
        backbufferTexture->Release();
    }
    else
    {
        LOGERROR("Failed to get backbuffer texture");
        success = false;
    }

    // Create default depth-stencil texture and view
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
    impl->device->CreateTexture2D(&depthDesc, 0, &impl->defaultDepthTexture);
    if (impl->defaultDepthTexture)
        impl->device->CreateDepthStencilView(impl->defaultDepthTexture, 0, &impl->defaultDepthStencilView);
    else
    {
        LOGERROR("Failed to create backbuffer depth-stencil texture");
        success = false;
    }

    // Update internally held backbuffer size and fullscreen state
    backbufferSize.x = width;
    backbufferSize.y = height;

    ResetRenderTargets();
    ResetViewport();
    return success;
}

void Graphics::HandleResize(WindowResizeEvent& /*event*/)
{
    // Handle window resize
    if (impl->swapChain && (window->Width() != backbufferSize.x || window->Height() != backbufferSize.y))
        UpdateSwapChain(window->Width(), window->Height());
}

void Graphics::PrepareDraw(PrimitiveType type)
{
    if (primitiveType != type)
    {
        impl->deviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
        primitiveType = type;
    }

    if (inputLayoutDirty && vertexShader)
    {
        InputLayoutDesc newInputLayout;
        newInputLayout.first = 0;
        newInputLayout.second = vertexShader->ElementHash();
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (vertexBuffers[i])
                newInputLayout.first |= (unsigned long long)vertexBuffers[i]->ElementHash() << (i * 16);
        }

        if (newInputLayout == inputLayout)
            return;

        inputLayoutDirty = false;

        // Check if layout already exists
        auto it = inputLayouts.Find(newInputLayout);
        if (it != inputLayouts.End())
        {
            impl->deviceContext->IASetInputLayout((ID3D11InputLayout*)it->second);
            inputLayout = newInputLayout;
            return;
        }

        {
            PROFILE(DefineInputLayout);
            
            // Not found, create new
            Vector<D3D11_INPUT_ELEMENT_DESC> elementDescs;

            for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
            {
                if (vertexBuffers[i])
                {
                    const Vector<VertexElement>& elements = vertexBuffers[i]->Elements();

                    for (size_t j = 0; j < elements.Size(); ++j)
                    {
                        const VertexElement& element = elements[j];
                        D3D11_INPUT_ELEMENT_DESC newDesc;
                        newDesc.SemanticName = VertexBuffer::elementSemantics[element.semantic];
                        newDesc.SemanticIndex = element.index;
                        newDesc.Format = d3dElementFormats[element.type];
                        newDesc.InputSlot = (unsigned)i;
                        newDesc.AlignedByteOffset = (unsigned)element.offset;
                        newDesc.InputSlotClass = element.perInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                        newDesc.InstanceDataStepRate = element.perInstance ? 1 : 0;
                        elementDescs.Push(newDesc);
                    }
                }
            }

            ID3D11InputLayout* d3dInputLayout = nullptr;
            ID3DBlob* d3dBlob = (ID3DBlob*)vertexShader->BlobObject();
            impl->device->CreateInputLayout(&elementDescs[0], (unsigned)elementDescs.Size(), d3dBlob->GetBufferPointer(),
                d3dBlob->GetBufferSize(), &d3dInputLayout);
            if (d3dInputLayout)
            {
                inputLayouts[newInputLayout] = d3dInputLayout;
                impl->deviceContext->IASetInputLayout(d3dInputLayout);
                inputLayout = newInputLayout;
            }
            else
                LOGERROR("Failed to create input layout");
        }
    }
}

void Graphics::ResetState()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers[i] = nullptr;
    
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            constantBuffers[i][j] = nullptr;
    }
    
    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures[i] = nullptr;
        impl->resourceViews[i] = nullptr;
        impl->samplers[i] = nullptr;
    }

    for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
        impl->renderTargetViews[i] = nullptr;

    indexBuffer = nullptr;
    vertexShader = nullptr;
    pixelShader = nullptr;
    blendState = nullptr;
    depthState = nullptr;
    rasterizerState = nullptr;
    impl->blendState = nullptr;
    impl->depthStencilState = nullptr;
    impl->rasterizerState = nullptr;
    impl->depthStencilView = nullptr;
    inputLayout.first = 0;
    inputLayout.second = 0;
    inputLayoutDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
    scissorRect = IntRect();
    stencilRef = 0;
}

void RegisterGraphicsLibrary()
{
    Shader::RegisterObject();
    Texture::RegisterObject();
}

}
