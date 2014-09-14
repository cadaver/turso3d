// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Window/Window.h"
#include "D3D11Graphics.h"

#include <d3d11.h>
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
        backbufferView(0)
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
};

Graphics::Graphics()
{
    RegisterSubsystem(this);
    impl = new GraphicsImpl();
    window = new Window();
}

Graphics::~Graphics()
{
    Close();
    RemoveSubsystem(this);
}

bool Graphics::SetMode(int width, int height, bool resizable)
{
    if (!window->SetSize(width, height, resizable))
        return false;
    
    return CreateDevice();
}

void Graphics::Close()
{
    if (impl->backbufferView)
    {
        impl->backbufferView->Release();
        impl->backbufferView = 0;
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
}

void Graphics::Present()
{
    if (!impl->device)
        return;

    impl->swapChain->Present(0, 0);
}

bool Graphics::CreateDevice()
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    memset(&swapChainDesc, 0, sizeof swapChainDesc);

    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = (HWND)window->Handle();
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

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

    // Get the backbuffer
    ID3D11Texture2D* backbufferTexture;
    impl->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbufferTexture);
    if (backbufferTexture)
    {
        impl->device->CreateRenderTargetView(backbufferTexture, NULL, &impl->backbufferView);
        backbufferTexture->Release();
        /// \todo Get depth stencil
        impl->deviceContext->OMSetRenderTargets(1, &impl->backbufferView, NULL);
    }
    else
        LOGERROR("Failed to get backbuffer texture");

    return true;
}

}
