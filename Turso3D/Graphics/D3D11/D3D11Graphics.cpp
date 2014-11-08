// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Window/Window.h"
#include "../GPUObject.h"
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
    /// Current D3D11 blend state object.
    ID3D11BlendState* blendState;
    /// Current D3D11 depth stencil state object.
    ID3D11DepthStencilState* depthStencilState;
    /// Current D3D11 rasterizer state object.
    ID3D11RasterizerState* rasterizerState;
    /// Current D3D11 shader resource views.
    ID3D11ShaderResourceView* resourceViews[MAX_TEXTURE_UNITS];
    /// Current D3D11 sampler states.
    ID3D11SamplerState* samplers[MAX_TEXTURE_UNITS];
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

    for (InputLayoutMap::Iterator it = inputLayouts.Begin(); it != inputLayouts.End(); ++it)
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

void Graphics::Present()
{
    impl->swapChain->Present(0, 0);
}

void Graphics::Clear(unsigned clearFlags, const Color& clearColor, float clearDepth, unsigned char clearStencil)
{
    if (clearFlags & CLEAR_COLOR)
        impl->deviceContext->ClearRenderTargetView(impl->backbufferView, clearColor.Data());
    if (clearFlags & (CLEAR_DEPTH|CLEAR_STENCIL))
    {
        unsigned depthClearFlags = 0;
        if (clearFlags & CLEAR_DEPTH)
            depthClearFlags |= D3D11_CLEAR_DEPTH;
        if (clearFlags & CLEAR_STENCIL)
            depthClearFlags |= D3D11_CLEAR_STENCIL;
        impl->deviceContext->ClearDepthStencilView(impl->depthStencilView, depthClearFlags, clearDepth, clearStencil);
    }
}

void Graphics::SetViewport(const IntRect& newViewport)
{
    /// \todo Test against current rendertarget once rendertarget switching is in place
    viewport.left = Clamp(newViewport.left, 0, backbufferSize.x - 1);
    viewport.top = Clamp(newViewport.top, 0, backbufferSize.y - 1);
    viewport.right = Clamp(newViewport.right, viewport.left + 1, backbufferSize.x);
    viewport.bottom = Clamp(newViewport.bottom, viewport.top + 1, backbufferSize.y);

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
    if (index < MAX_VERTEX_STREAMS && vertexBuffers[index] != buffer)
    {
        vertexBuffers[index] = buffer;
        ID3D11Buffer* d3dBuffer = buffer ? (ID3D11Buffer*)buffer->BufferObject() : (ID3D11Buffer*)0;
        unsigned stride = buffer ? (unsigned)buffer->VertexSize() : 0;
        unsigned offset = 0;
        impl->deviceContext->IASetVertexBuffers((unsigned)index, 1, &d3dBuffer, &stride, &offset);
        inputLayoutDirty = true;
    }
}

void Graphics::SetConstantBuffer(ShaderStage stage, size_t index, ConstantBuffer* buffer)
{
    if (stage < MAX_SHADER_STAGES &&index < MAX_CONSTANT_BUFFERS && constantBuffers[stage][index] != buffer)
    {
        constantBuffers[stage][index] = buffer;
        ID3D11Buffer* d3dBuffer = buffer ? (ID3D11Buffer*)buffer->BufferObject() : (ID3D11Buffer*)0;
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
        ID3D11ShaderResourceView* d3dResourceView = texture ? (ID3D11ShaderResourceView*)texture->ResourceViewObject() :
            (ID3D11ShaderResourceView*)0;
        ID3D11SamplerState* d3dSampler = texture ? (ID3D11SamplerState*)texture->SamplerObject() : (ID3D11SamplerState*)0;
        // Note: now both VS & PS resource views are set at the same time, to mimic OpenGL conventions
        if (impl->resourceViews[index] != d3dResourceView)
        {
            impl->resourceViews[index] = d3dResourceView;
            impl->deviceContext->VSSetShaderResources((unsigned)index, 1, &d3dResourceView);
            impl->deviceContext->PSSetShaderResources((unsigned)index, 1, &d3dResourceView);
        }
        if (impl->samplers[index] != d3dSampler)
        {
            impl->samplers[index] = d3dSampler;
            impl->deviceContext->VSSetSamplers((unsigned)index, 1, &d3dSampler);
            impl->deviceContext->PSSetSamplers((unsigned)index, 1, &d3dSampler);
        }
    }
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (indexBuffer != buffer)
    {
        indexBuffer = buffer;
        if (buffer)
            impl->deviceContext->IASetIndexBuffer((ID3D11Buffer*)buffer->BufferObject(), buffer->IndexSize() ==
                sizeof(unsigned short) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0);
        else
            impl->deviceContext->IASetIndexBuffer(0, DXGI_FORMAT_UNKNOWN, 0);
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

void Graphics::SetBlendState(BlendState* state)
{
    if (state != blendState)
    {
        ID3D11BlendState* d3dBlendState = state ? (ID3D11BlendState*)state->StateObject() : (ID3D11BlendState*)0;
        if (d3dBlendState != impl->blendState)
        {
            impl->deviceContext->OMSetBlendState(d3dBlendState, 0, 0xffffffff);
            impl->blendState = d3dBlendState;
        }
        blendState = state;
    }
}

void Graphics::SetDepthState(DepthState* state, unsigned newStencilRef)
{
    if (state != depthState || newStencilRef != stencilRef)
    {
        ID3D11DepthStencilState* d3dDepthStencilState = state ? (ID3D11DepthStencilState*)state->StateObject() : (ID3D11DepthStencilState*)0;
        if (d3dDepthStencilState != impl->depthStencilState || newStencilRef != stencilRef)
        {
            impl->deviceContext->OMSetDepthStencilState(d3dDepthStencilState, newStencilRef);
            impl->depthStencilState = d3dDepthStencilState;
            stencilRef = newStencilRef;
        }
        depthState = state;
    }
}

void Graphics::SetRasterizerState(RasterizerState* state)
{
    if (state != rasterizerState)
    {
        ID3D11RasterizerState* d3dRasterizerState = state ? (ID3D11RasterizerState*)state->StateObject() : (ID3D11RasterizerState*)0;
        if (d3dRasterizerState != impl->rasterizerState)
        {
            impl->deviceContext->RSSetState(d3dRasterizerState);
            impl->rasterizerState = d3dRasterizerState;
        }
        rasterizerState = state;
    }
}

void Graphics::SetScissorRect(const IntRect& newScissorRect)
{
    if (newScissorRect != scissorRect)
    {
        /// \todo Test against current rendertarget once rendertarget switching is in place
        scissorRect.left = Clamp(newScissorRect.left, 0, backbufferSize.x - 1);
        scissorRect.top = Clamp(newScissorRect.top, 0, backbufferSize.y - 1);
        scissorRect.right = Clamp(newScissorRect.right, scissorRect.left + 1, backbufferSize.x);
        scissorRect.bottom = Clamp(newScissorRect.bottom, scissorRect.top + 1, backbufferSize.y);

        D3D11_RECT d3dRect;
        d3dRect.left = scissorRect.left;
        d3dRect.top = scissorRect.top;
        d3dRect.right = scissorRect.right;
        d3dRect.bottom = scissorRect.bottom;
        impl->deviceContext->RSSetScissorRects(1, &d3dRect);
    }
}

void Graphics::ResetVertexBuffers()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        SetVertexBuffer(i, 0);
}

void Graphics::ResetConstantBuffers()
{
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; i < MAX_CONSTANT_BUFFERS; ++j)
            SetConstantBuffer((ShaderStage)i, j, 0);
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

VertexBuffer* Graphics::GetVertexBuffer(size_t index) const
{
    return index < MAX_VERTEX_STREAMS ? vertexBuffers[index] : (VertexBuffer*)0;
}

ConstantBuffer* Graphics::GetConstantBuffer(ShaderStage stage, size_t index) const
{
    return (stage < MAX_SHADER_STAGES && index < MAX_CONSTANT_BUFFERS) ? constantBuffers[stage][index] : (ConstantBuffer*)0;
}

Texture* Graphics::GetTexture(size_t index) const
{
    return (index < MAX_TEXTURE_UNITS) ? textures[index] : (Texture*)0;
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
    impl->swapChain->GetParent(IID_IDXGIFactory, (void**)&factory);
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
    impl->swapChain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&backbufferTexture);
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

    // Update internally held backbuffer size and fullscreen state
    backbufferSize.x = width;
    backbufferSize.y = height;
    fullscreen = fullscreen_;

    impl->deviceContext->OMSetRenderTargets(1, &impl->backbufferView, impl->depthStencilView);
    SetViewport(IntRect(0, 0, width, height));
    
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
        impl->deviceContext->IASetPrimitiveTopology((D3D11_PRIMITIVE_TOPOLOGY)type);
        primitiveType = type;
    }

    if (inputLayoutDirty && vertexShader)
    {
        InputLayoutDesc newInputLayout;
        newInputLayout.first = 0;
        newInputLayout.second = vertexShader->ElementMask();
        for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        {
            if (vertexBuffers[i])
                newInputLayout.first |= (unsigned long long)vertexBuffers[i]->ElementMask() << (i * MAX_VERTEX_ELEMENTS);
        }

        if (newInputLayout == inputLayout)
            return;

        inputLayoutDirty = false;

        // Check if layout already exists
        InputLayoutMap::ConstIterator it = inputLayouts.Find(newInputLayout);
        if (it != inputLayouts.End())
        {
            impl->deviceContext->IASetInputLayout((ID3D11InputLayout*)it->second);
            inputLayout = newInputLayout;
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
                        newDesc.SemanticName = VertexBuffer::elementSemantic[j];
                        newDesc.SemanticIndex = VertexBuffer::elementSemanticIndex[j];
                        newDesc.Format = (DXGI_FORMAT)VertexBuffer::elementFormat[j];
                        newDesc.InputSlot = (unsigned)i;
                        newDesc.AlignedByteOffset = (unsigned)currentOffset;
                        newDesc.InputSlotClass = j < ELEMENT_INSTANCEMATRIX1 ? D3D11_INPUT_PER_VERTEX_DATA :  D3D11_INPUT_PER_INSTANCE_DATA;
                        newDesc.InstanceDataStepRate = j < ELEMENT_INSTANCEMATRIX1 ? 0 : 1;
                        elementDescs.Push(newDesc);

                        currentOffset += VertexBuffer::elementSize[j];
                    }
                }
            }
        }

        ID3D11InputLayout* d3dInputLayout = 0;
        ID3DBlob* d3dBlob = (ID3DBlob*)vertexShader->CompiledBlob();
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

void Graphics::ResetState()
{
    for (size_t i = 0; i < MAX_VERTEX_STREAMS; ++i)
        vertexBuffers[i] = 0;
    
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            constantBuffers[i][j] = 0;
    }
    
    for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
    {
        textures[i] = 0;
        impl->resourceViews[i] = 0;
        impl->samplers[i] = 0;
    }

    indexBuffer = 0;
    vertexShader = 0;
    pixelShader = 0;
    blendState = 0;
    depthState = 0;
    rasterizerState = 0;
    impl->blendState = 0;
    impl->depthStencilState = 0;
    impl->rasterizerState = 0;
    inputLayout.first = 0;
    inputLayout.second = 0;
    inputLayoutDirty = false;
    primitiveType = MAX_PRIMITIVE_TYPES;
    scissorRect = IntRect();
    stencilRef = 0;
}

void RegisterGraphicsLibrary()
{
    Texture::RegisterObject();
}

}
