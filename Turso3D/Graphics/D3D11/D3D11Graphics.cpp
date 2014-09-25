// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Window/Window.h"
#include "D3D11Graphics.h"

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
};

Graphics::Graphics() :
    backbufferSize(IntVector2::ZERO),
    fullscreen(false)
{
    RegisterSubsystem(this);
    impl = new GraphicsImpl();
    window = new Window();
    SubscribeToEvent(window->resizeEvent, &Graphics::HandleResize);
}

Graphics::~Graphics()
{
    Close();
    RemoveSubsystem(this);
}

bool Graphics::SetMode(int width, int height, bool fullscreen, bool resizable)
{
    // Setting window size only required if window not open yet, otherwise the swapchain
    // takes care of resizing
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
}

bool Graphics::IsInitialized() const
{
    return window != 0 && impl->device != 0;
}

Window* Graphics::RenderWindow() const
{
    return window;
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
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        NULL,
        NULL,
        NULL,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &impl->swapChain,
        &impl->device,
        NULL,
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
    bool success = true;

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
        impl->swapChain->SetFullscreenState(fullscreen_, NULL);
    }
    else
    {
        impl->swapChain->SetFullscreenState(fullscreen_, NULL);
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
        impl->device->CreateRenderTargetView(backbufferTexture, NULL, &impl->backbufferView);
        backbufferTexture->Release();
    }
    else
    {
        LOGERROR("Failed to get backbuffer texture");
        success = false;
    }

    if (impl->depthTexture)
    {
        impl->depthTexture->Release();
        impl->depthTexture = 0;
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
    impl->device->CreateTexture2D(&depthDesc, NULL, &impl->depthTexture);
    if (impl->depthTexture)
        impl->device->CreateDepthStencilView(impl->depthTexture, NULL, &impl->depthStencilView);
    else
    {
        LOGERROR("Failed to create depth texture");
        success = false;
    }

    impl->deviceContext->OMSetRenderTargets(1, &impl->backbufferView, impl->depthStencilView);

    // Update internally held backbuffer size and fullscreen state
    backbufferSize.x = width;
    backbufferSize.y = height;
    fullscreen = fullscreen_;

    return success;
}

void Graphics::HandleResize(WindowResizeEvent& /*event*/)
{
    // Handle windowed mode resize
    if (impl->swapChain && !fullscreen && (window->Width() != backbufferSize.x || window->Height() != backbufferSize.y))
        UpdateSwapChain(window->Width(), window->Height(), false);
}

}
