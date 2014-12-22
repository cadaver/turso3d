// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Window/Window.h"
#include "../GPUObject.h"
#include "../Shader.h"
#include "D3D11Graphics.h"
#include "D3D11ConstantBuffer.h"
#include "D3D11IndexBuffer.h"
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

/// \cond PRIVATE
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
        blendState(nullptr),
        depthState(nullptr),
        rasterizerState(nullptr),
        depthStencilView(nullptr),
        blendStateHash(0xffffffffffffffff),
        depthStateHash(0xffffffffffffffff),
        rasterizerStateHash(0xffffffffffffffff),
        stencilRef(0)
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
    /// Current depth state object.
    ID3D11DepthStencilState* depthState;
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
    /// Stencil ref value set to the device.
    unsigned char stencilRef;
    /// Current blend state hash code.
    unsigned long long blendStateHash;
    /// Current depth state hash code.
    unsigned long long depthStateHash;
    /// Current rasterizer state hash code.
    unsigned long long rasterizerStateHash;
};
/// \endcond

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    renderTargetSize(IntVector2::ZERO),
    multisample(1),
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

bool Graphics::SetMode(const IntVector2& size, bool fullscreen, bool resizable, int multisample_)
{
    multisample_ = Clamp(multisample_, 1, 16);

    if (!window->SetSize(size, fullscreen, resizable))
        return false;

    // Create D3D11 device and swap chain when setting mode for the first time, or swap chain again when changing multisample
    if (!impl->device || multisample_ != multisample)
    {
        if (!CreateD3DDevice(multisample_))
            return false;
        // Swap chain needs to be updated manually for the first time, otherwise window resize event takes care of it
        UpdateSwapChain(window->Width(), window->Height());
    }

    screenModeEvent.size = backbufferSize;
    screenModeEvent.fullscreen = IsFullscreen();
    screenModeEvent.resizable = IsResizable();
    screenModeEvent.multisample = multisample;
    SendEvent(screenModeEvent);

    LOGDEBUGF("Set screen mode %dx%d fullscreen %d resizable %d multisample %d", backbufferSize.x, backbufferSize.y,
        IsFullscreen(), IsResizable(), multisample);

    return true;
}

bool Graphics::SetFullscreen(bool enable)
{
    if (!IsInitialized())
        return false;
    else
        return SetMode(backbufferSize, enable, window->IsResizable(), multisample);
}

bool Graphics::SetMultisample(int multisample_)
{
    if (!IsInitialized())
        return false;
    else
        return SetMode(backbufferSize, window->IsFullscreen(), window->IsResizable(), multisample_);
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

    for (auto it = blendStates.Begin(); it != blendStates.End(); ++it)
    {
        ID3D11BlendState* d3dState = (ID3D11BlendState*)it->second;
        d3dState->Release();
    }
    blendStates.Clear();

    for (auto it = depthStates.Begin(); it != depthStates.End(); ++it)
    {
        ID3D11DepthStencilState* d3dState = (ID3D11DepthStencilState*)it->second;
        d3dState->Release();
    }
    depthStates.Clear();

    for (auto it = rasterizerStates.Begin(); it != rasterizerStates.End(); ++it)
    {
        ID3D11RasterizerState* d3dState = (ID3D11RasterizerState*)it->second;
        d3dState->Release();
    }
    rasterizerStates.Clear();

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
    ResetState();
}

void Graphics::Present()
{
    impl->swapChain->Present(vsync ? 1 : 0, 0);
}

void Graphics::SetRenderTarget(Texture* renderTarget_, Texture* depthStencil_)
{
    static Vector<Texture*> renderTargetVector(1);
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

void Graphics::SetColorState(const BlendModeDesc& blendMode, bool alphaToCoverage, unsigned char colorWriteMask)
{
    renderState.blendMode = blendMode;
    renderState.colorWriteMask = colorWriteMask;
    renderState.alphaToCoverage = alphaToCoverage;
    
    blendStateDirty = true;
}

void Graphics::SetColorState(BlendMode blendMode, bool alphaToCoverage, unsigned char colorWriteMask)
{
    renderState.blendMode = blendModes[blendMode];
    renderState.colorWriteMask = colorWriteMask;
    renderState.alphaToCoverage = alphaToCoverage;

    blendStateDirty = true;
}

void Graphics::SetDepthState(CompareFunc depthFunc, bool depthWrite, bool depthClip, int depthBias, float depthBiasClamp,
    float slopeScaledDepthBias)
{
    renderState.depthFunc = depthFunc;
    renderState.depthWrite = depthWrite;
    renderState.depthClip = depthClip;
    renderState.depthBias = depthBias;
    renderState.depthBiasClamp = depthBiasClamp;
    renderState.slopeScaledDepthBias = slopeScaledDepthBias;

    depthStateDirty = true;
    rasterizerStateDirty = true;
}

void Graphics::SetRasterizerState(CullMode cullMode, FillMode fillMode)
{
    renderState.cullMode = cullMode;
    renderState.fillMode = fillMode;

    rasterizerStateDirty = true;
}

void Graphics::SetScissorTest(bool scissorEnable, const IntRect& scissorRect)
{
    renderState.scissorEnable = scissorEnable;

    if (scissorRect != renderState.scissorRect)
    {
        /// \todo Implement a member function in IntRect for clipping
        renderState.scissorRect.left = Clamp(scissorRect.left, 0, renderTargetSize.x - 1);
        renderState.scissorRect.top = Clamp(scissorRect.top, 0, renderTargetSize.y - 1);
        renderState.scissorRect.right = Clamp(scissorRect.right, renderState.scissorRect.left + 1, renderTargetSize.x);
        renderState.scissorRect.bottom = Clamp(scissorRect.bottom, renderState.scissorRect.top + 1, renderTargetSize.y);

        D3D11_RECT d3dRect;
        d3dRect.left = renderState.scissorRect.left;
        d3dRect.top = renderState.scissorRect.top;
        d3dRect.right = renderState.scissorRect.right;
        d3dRect.bottom = renderState.scissorRect.bottom;
        impl->deviceContext->RSSetScissorRects(1, &d3dRect);
    }

    rasterizerStateDirty = true;
}

void Graphics::SetStencilTest(bool stencilEnable, const StencilTestDesc& stencilTest, unsigned char stencilRef)
{
    renderState.stencilEnable = stencilEnable;
    // When stencil test is disabled, always set default stencil test parameters to prevent creating unnecessary states
    renderState.stencilTest = stencilEnable ? stencilTest : StencilTestDesc();
    renderState.stencilRef = stencilRef;

    depthStateDirty = true;
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

void Graphics::DrawIndexed(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart)
{
    PrepareDraw(type);
    impl->deviceContext->DrawIndexed((unsigned)indexCount, (unsigned)indexStart, (unsigned)vertexStart);
}

void Graphics::DrawInstanced(PrimitiveType type, size_t vertexStart, size_t vertexCount, size_t instanceStart, size_t instanceCount)
{
    PrepareDraw(type);
    impl->deviceContext->DrawInstanced((unsigned)vertexCount, (unsigned)instanceCount, (unsigned)vertexStart, (unsigned)instanceStart);
}

void Graphics::DrawIndexedInstanced(PrimitiveType type, size_t indexStart, size_t indexCount, size_t vertexStart, size_t instanceStart, size_t instanceCount)
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

bool Graphics::CreateD3DDevice(int multisample_)
{
    // Device needs only to be created once
    if (!impl->device)
    {
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
    }

    // Create swap chain. Release old if necessary
    if (impl->swapChain)
    {
        impl->swapChain->Release();
        impl->swapChain = nullptr;
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    memset(&swapChainDesc, 0, sizeof swapChainDesc);
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = window->Width();
    swapChainDesc.BufferDesc.Height = window->Height();
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = (HWND)window->Handle();
    swapChainDesc.SampleDesc.Count = multisample_;
    swapChainDesc.SampleDesc.Quality = multisample_ > 1 ? 0xffffffff : 0;
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

    if (impl->swapChain)
    {
        multisample = multisample_;
        return true;
    }
    else
    {
        LOGERROR("Failed to create D3D11 swap chain");
        return false;
    }
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
    depthDesc.SampleDesc.Count = multisample;
    depthDesc.SampleDesc.Quality = multisample > 1 ? 0xffffffff : 0;
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
                        newDesc.SemanticName = elementSemanticNames[element.semantic];
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

    if (blendStateDirty)
    {
        unsigned long long blendStateHash = 
            renderState.colorWriteMask |
            (renderState.alphaToCoverage ? 0x10 : 0x0) |
            (renderState.blendMode.blendEnable ? 0x20 : 0x0) |
            (renderState.blendMode.srcBlend << 6) |
            (renderState.blendMode.destBlend << 10) |
            (renderState.blendMode.blendOp << 14) |
            (renderState.blendMode.srcBlendAlpha << 17) |
            (renderState.blendMode.destBlendAlpha << 21) |
            (renderState.blendMode.blendOpAlpha << 25);

        if (blendStateHash != impl->blendStateHash)
        {
            auto it = blendStates.Find(blendStateHash);
            if (it != blendStates.End())
            {
                ID3D11BlendState* newBlendState = (ID3D11BlendState*)it->second;
                impl->deviceContext->OMSetBlendState(newBlendState, nullptr, 0xffffffff);
                impl->blendState = newBlendState;
                impl->blendStateHash = blendStateHash;
            }
            else
            {
                PROFILE(CreateBlendState);
    
                D3D11_BLEND_DESC stateDesc;
                memset(&stateDesc, 0, sizeof stateDesc);
    
                stateDesc.AlphaToCoverageEnable = renderState.alphaToCoverage;
                stateDesc.IndependentBlendEnable = false;
                stateDesc.RenderTarget[0].BlendEnable = renderState.blendMode.blendEnable;
                stateDesc.RenderTarget[0].SrcBlend = (D3D11_BLEND)renderState.blendMode.srcBlend;
                stateDesc.RenderTarget[0].DestBlend = (D3D11_BLEND)renderState.blendMode.destBlend;
                stateDesc.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)renderState.blendMode.blendOp;
                stateDesc.RenderTarget[0].SrcBlendAlpha = (D3D11_BLEND)renderState.blendMode.srcBlendAlpha;
                stateDesc.RenderTarget[0].DestBlendAlpha = (D3D11_BLEND)renderState.blendMode.destBlendAlpha;
                stateDesc.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP)renderState.blendMode.blendOpAlpha;
                stateDesc.RenderTarget[0].RenderTargetWriteMask = renderState.colorWriteMask & COLORMASK_ALL;
    
                ID3D11BlendState* newBlendState = nullptr;
                impl->device->CreateBlendState(&stateDesc, &newBlendState);
                if (newBlendState)
                {
                    impl->deviceContext->OMSetBlendState(newBlendState, nullptr, 0xffffffff);
                    impl->blendState = newBlendState;
                    impl->blendStateHash = blendStateHash;
                    blendStates[blendStateHash] = newBlendState;
                    
                    LOGDEBUGF("Created new blend state with hash %x", blendStateHash & 0xffffffff);
                }
            }
        }

        blendStateDirty = false;
    }

    if (depthStateDirty)
    {
        unsigned long long depthStateHash = 
            (renderState.depthWrite ? 0x1 : 0x0) |
            (renderState.stencilEnable ? 0x2 : 0x0) |
            (renderState.depthFunc << 2) |
            (renderState.stencilTest.stencilReadMask << 6) |
            (renderState.stencilTest.stencilWriteMask << 14) |
            (renderState.stencilTest.frontFail << 22) |
            (renderState.stencilTest.frontDepthFail << 26) |
            ((unsigned long long)renderState.stencilTest.frontPass << 30) |
            ((unsigned long long)renderState.stencilTest.frontFunc << 34) |
            ((unsigned long long)renderState.stencilTest.frontFail << 38) |
            ((unsigned long long)renderState.stencilTest.frontDepthFail << 42) |
            ((unsigned long long)renderState.stencilTest.frontPass << 46) |
            ((unsigned long long)renderState.stencilTest.frontFunc << 50);

        if (depthStateHash != impl->depthStateHash || renderState.stencilRef != impl->stencilRef)
        {
            auto it = depthStates.Find(depthStateHash);
            if (it != depthStates.End())
            {
                ID3D11DepthStencilState* newDepthState = (ID3D11DepthStencilState*)it->second;
                impl->deviceContext->OMSetDepthStencilState(newDepthState, renderState.stencilRef);
                impl->depthState = newDepthState;
                impl->depthStateHash = depthStateHash;
                impl->stencilRef = renderState.stencilRef;
            }
            else
            {
                PROFILE(CreateDepthStencilState);
    
                D3D11_DEPTH_STENCIL_DESC stateDesc;
                memset(&stateDesc, 0, sizeof stateDesc);
    
                stateDesc.DepthEnable = TRUE;
                stateDesc.DepthWriteMask = renderState.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
                stateDesc.DepthFunc = (D3D11_COMPARISON_FUNC)renderState.depthFunc;
                stateDesc.StencilEnable = renderState.stencilEnable;
                stateDesc.StencilReadMask = renderState.stencilTest.stencilReadMask;
                stateDesc.StencilWriteMask = renderState.stencilTest.stencilWriteMask;
                stateDesc.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)renderState.stencilTest.frontFail;
                stateDesc.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)renderState.stencilTest.frontDepthFail;
                stateDesc.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)renderState.stencilTest.frontPass;
                stateDesc.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)renderState.stencilTest.frontFunc;
                stateDesc.BackFace.StencilFailOp = (D3D11_STENCIL_OP)renderState.stencilTest.backFail;
                stateDesc.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)renderState.stencilTest.backDepthFail;
                stateDesc.BackFace.StencilPassOp = (D3D11_STENCIL_OP)renderState.stencilTest.backPass;
                stateDesc.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)renderState.stencilTest.backFunc;
                
                ID3D11DepthStencilState* newDepthState = nullptr;
                impl->device->CreateDepthStencilState(&stateDesc, &newDepthState);
                if (newDepthState)
                {
                    impl->deviceContext->OMSetDepthStencilState(newDepthState, renderState.stencilRef);
                    impl->depthState = newDepthState;
                    impl->depthStateHash = depthStateHash;
                    impl->stencilRef = renderState.stencilRef;
                    depthStates[depthStateHash] = newDepthState;
                
                    LOGDEBUGF("Created new depth state with hash %x", depthStateHash & 0xffffffff);
                }
            }
        }

        depthStateDirty = false;
    }

    if (rasterizerStateDirty)
    {
        unsigned long long rasterizerStateHash =
            (renderState.depthClip ? 0x1 : 0x0) |
            (renderState.scissorEnable ? 0x2 : 0x0) |
            (renderState.fillMode << 2) |
            (renderState.cullMode << 4) |
            ((renderState.depthBias & 0xff) << 6) |
            ((unsigned long long)(*((unsigned*)&renderState.depthBiasClamp) >> 8) << 14) |
            ((unsigned long long)(*((unsigned*)&renderState.slopeScaledDepthBias) >> 8) << 38);

        if (rasterizerStateHash != impl->rasterizerStateHash)
        {
            auto it = rasterizerStates.Find(rasterizerStateHash);
            if (it != rasterizerStates.End())
            {
                ID3D11RasterizerState* newRasterizerState = (ID3D11RasterizerState*)it->second;
                impl->deviceContext->RSSetState(newRasterizerState);
                impl->rasterizerState = newRasterizerState;
                impl->rasterizerStateHash = rasterizerStateHash;
            }
            else
            {
                PROFILE(CreateRasterizerState);
                
                D3D11_RASTERIZER_DESC stateDesc;
                memset(&stateDesc, 0, sizeof stateDesc);
                
                stateDesc.FillMode = (D3D11_FILL_MODE)renderState.fillMode;
                stateDesc.CullMode = (D3D11_CULL_MODE)renderState.cullMode;
                stateDesc.FrontCounterClockwise = FALSE;
                stateDesc.DepthBias = renderState.depthBias;
                stateDesc.DepthBiasClamp = renderState.depthBiasClamp;
                stateDesc.SlopeScaledDepthBias = renderState.slopeScaledDepthBias;
                stateDesc.DepthClipEnable = renderState.depthClip;
                stateDesc.ScissorEnable = renderState.scissorEnable;
                stateDesc.MultisampleEnable = TRUE;
                stateDesc.AntialiasedLineEnable = FALSE;
                
                ID3D11RasterizerState* newRasterizerState = nullptr;
                impl->device->CreateRasterizerState(&stateDesc, &newRasterizerState);
                if (newRasterizerState)
                {
                    impl->deviceContext->RSSetState(newRasterizerState);
                    impl->rasterizerState = newRasterizerState;
                    impl->rasterizerStateHash = rasterizerStateHash;
                    rasterizerStates[rasterizerStateHash] = newRasterizerState;
                    
                    LOGDEBUGF("Created new rasterizer state with hash %x", rasterizerStateHash & 0xffffffff);
                }
            }
        }

        rasterizerStateDirty = false;
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

    renderState.Reset();

    indexBuffer = nullptr;
    vertexShader = nullptr;
    pixelShader = nullptr;
    impl->blendState = nullptr;
    impl->depthState = nullptr;
    impl->rasterizerState = nullptr;
    impl->depthStencilView = nullptr;
    impl->blendStateHash = 0xffffffffffffffff;
    impl->depthStateHash = 0xffffffffffffffff;
    impl->rasterizerStateHash = 0xffffffffffffffff;
    impl->stencilRef = 0;
    inputLayout.first = 0;
    inputLayout.second = 0;
    inputLayoutDirty = false;
    blendStateDirty = false;
    depthStateDirty = false;
    rasterizerStateDirty = false;
    scissorRectDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
}

void RegisterGraphicsLibrary()
{
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    Shader::RegisterObject();
    Texture::RegisterObject();
}

}
