// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11Graphics.h"
#include "D3D11Texture.h"

#include <d3d11.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

static const D3D11_FILTER filterMode[] = 
{
    D3D11_FILTER_MIN_MAG_MIP_POINT,
    D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_MIN_MAG_MIP_LINEAR,
    D3D11_FILTER_ANISOTROPIC
};

static const DXGI_FORMAT textureFormat[] = 
{
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_A8_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R16_FLOAT,
    DXGI_FORMAT_R16G16_FLOAT,
    DXGI_FORMAT_R16G16B16A16_FLOAT,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC3_UNORM
};

static const D3D11_USAGE textureUsage[] =
{
    D3D11_USAGE_IMMUTABLE,
    D3D11_USAGE_DYNAMIC,
    D3D11_USAGE_DEFAULT
};

Texture::Texture() :
    texture(0),
    resourceView(0),
    sampler(0)
{
}

Texture::~Texture()
{
    Release();
}

void Texture::RegisterObject()
{
    RegisterFactory<Texture>();
}

bool Texture::BeginLoad(Stream& source)
{
    Image* image = new Image();
    if (image->Load(source))
        loadImages.Push(image);
    else
    {
        delete image;
        return false;
    }

    // Construct mip levels now if image is uncompressed
    if (!image->IsCompressed())
    {
        while (image->Width() > 1 || image->Height() > 1)
        {
            Image* nextMipImage = new Image();
            if (image->GenerateMipImage(*nextMipImage))
            {
                loadImages.Push(nextMipImage);
                image = nextMipImage;
            }
            else
            {
                delete nextMipImage;
                break;
            }
        }
    }

    return true;
}

bool Texture::EndLoad()
{
    if (loadImages.IsEmpty())
        return false;

    Vector<ImageLevel> initialData;

    for (size_t i = 0; i < loadImages.Size(); ++i)
    {
        for (size_t j = 0; j < loadImages[i]->NumLevels(); ++j)
            initialData.Push(loadImages[i]->Level(j));
    }

    Image* image = loadImages[0];
    bool success = Define(TEX_2D, USAGE_STATIC, image->Width(), image->Height(), image->Format(), initialData.Size(), &initialData[0]);
    /// \todo Read a parameter file for the sampling parameters
    success &= DefineSampler(FILTER_TRILINEAR, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP, 16, 0.0f, D3D11_FLOAT32_MAX, Color::BLACK);

    for (size_t i = 0; i < loadImages.Size(); ++i)
        delete loadImages[i];
    loadImages.Clear();

    return success;
}

void Texture::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
        {
            if (graphics->GetTexture(i) == this)
                graphics->SetTexture(i, 0);
        }

        if (usage == USAGE_RENDERTARGET)
        {
            bool clear = false;

            for (size_t i = 0; i < MAX_RENDERTARGETS; ++i)
            {
                if (graphics->RenderTarget(i) == this)
                {
                    clear = true;
                    break;
                }
            }

            if (!clear && graphics->DepthStencil() == this)
                clear = true;

            if (clear)
                graphics->ResetRenderTargets();
        }
    }

    if (resourceView)
    {
        ID3D11ShaderResourceView* d3dResourceView = (ID3D11ShaderResourceView*)resourceView;
        d3dResourceView->Release();
        resourceView = 0;
    }
    if (IsRenderTarget())
    {
        ID3D11RenderTargetView* d3dRenderTargetView = (ID3D11RenderTargetView*)renderTargetView;
        d3dRenderTargetView->Release();
        renderTargetView = 0;
    }
    if (IsDepthStencil())
    {
        ID3D11DepthStencilView* d3dDepthStencilView = (ID3D11DepthStencilView*)renderTargetView;
        d3dDepthStencilView->Release();
        renderTargetView = 0;
    }
    if (sampler)
    {
        ID3D11SamplerState* d3dSampler = (ID3D11SamplerState*)sampler;
        d3dSampler->Release();
        sampler = 0;
    }
    if (texture)
    {
        ID3D11Resource* d3dTexture = (ID3D11Resource*)texture;
        d3dTexture->Release();
        texture = 0;
    }
}

bool Texture::Define(TextureType type_, TextureUsage usage_, int width_, int height_, ImageFormat format_, size_t numLevels_, const ImageLevel* initialData)
{
    PROFILE(DefineTexture);

    Release();

    if (type_ != TEX_2D)
    {
        LOGERROR("Only 2D textures supported for now");
        return false;
    }

    if (format_ > FMT_DXT5)
    {
        LOGERROR("ETC1 and PVRTC formats are unsupported");
        return false;
    }

    if (numLevels_ < 1)
        numLevels_ = 1;

    type = type_;
    usage = usage_;

    if (graphics && graphics->IsInitialized())
    {
        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();

        D3D11_TEXTURE2D_DESC textureDesc;
        memset(&textureDesc, 0, sizeof textureDesc);
        textureDesc.Width = width_;
        textureDesc.Height = height_;
        textureDesc.MipLevels = (unsigned)numLevels_;
        textureDesc.ArraySize = 1;
        textureDesc.Format = textureFormat[format_];
        /// \todo Support defining multisampled textures
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = textureUsage[usage_];
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (usage == USAGE_RENDERTARGET)
        {
            if (format_ < FMT_D16 || format_ > FMT_D24S8)
                textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            else
                textureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        }
        textureDesc.CPUAccessFlags = usage == USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;

        Vector<D3D11_SUBRESOURCE_DATA> subResourceData;
        if (initialData)
        {
            subResourceData.Resize(numLevels_);
            for (size_t i = 0; i < numLevels_; ++i)
            {
                subResourceData[i].pSysMem = initialData[i].data;
                subResourceData[i].SysMemPitch = (unsigned)initialData[i].rowSize;
                subResourceData[i].SysMemSlicePitch = 0;
            }
        }

        d3dDevice->CreateTexture2D(&textureDesc, subResourceData.Size() ? &subResourceData[0] : (D3D11_SUBRESOURCE_DATA*)0, (ID3D11Texture2D**)&texture);
        if (!texture)
        {
            width = 0;
            height = 0;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }
        else
        {
            width = width_;
            height = height_;
            format = format_;
            numLevels = numLevels_;

            LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", width, height, (int)format, numLevels);
        }

        if (IsRenderTarget())
        {
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
            memset(&renderTargetViewDesc, 0, sizeof renderTargetViewDesc);
            renderTargetViewDesc.Format = textureDesc.Format;
            renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            d3dDevice->CreateRenderTargetView((ID3D11Resource*)texture, &renderTargetViewDesc, (ID3D11RenderTargetView**)&renderTargetView);
            if (!renderTargetView)
                LOGERROR("Failed to create rendertarget view for texture");
        }
        else if (IsDepthStencil())
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
            memset(&depthStencilViewDesc, 0, sizeof depthStencilViewDesc);
            depthStencilViewDesc.Format = textureDesc.Format;
            depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            depthStencilViewDesc.Flags = 0;

            d3dDevice->CreateDepthStencilView((ID3D11Resource*)texture, &depthStencilViewDesc, (ID3D11DepthStencilView**)&renderTargetView);
            if (!renderTargetView)
                LOGERROR("Failed to create depth-stencil view for texture");
        }

        if (textureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
            memset(&resourceViewDesc, 0, sizeof resourceViewDesc);
            resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            resourceViewDesc.Texture2D.MipLevels = (unsigned)numLevels;
            resourceViewDesc.Texture2D.MostDetailedMip = 0;
            resourceViewDesc.Format = textureDesc.Format;

            d3dDevice->CreateShaderResourceView((ID3D11Resource*)texture, &resourceViewDesc, (ID3D11ShaderResourceView**)&resourceView);
            if (!resourceView)
                LOGERROR("Failed to create shader resource view for texture");
        }
    }

    return true;
}

bool Texture::DefineSampler(TextureFilterMode filter, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy, float minLod, float maxLod, const Color& borderColor)
{
    PROFILE(DefineTextureSampler);

    // Release the previous sampler first
    if (sampler)
    {
        ID3D11SamplerState* d3dSampler = (ID3D11SamplerState*)sampler;
        d3dSampler->Release();
        sampler = 0;
    }

    if (graphics && graphics->IsInitialized())
    {
        D3D11_SAMPLER_DESC samplerDesc;
        memset(&samplerDesc, 0, sizeof samplerDesc);
        samplerDesc.Filter = filterMode[filter];
        samplerDesc.AddressU = (D3D11_TEXTURE_ADDRESS_MODE)u;
        samplerDesc.AddressV = (D3D11_TEXTURE_ADDRESS_MODE)v;
        samplerDesc.AddressW = (D3D11_TEXTURE_ADDRESS_MODE)w;
        samplerDesc.MaxAnisotropy = maxAnisotropy;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = minLod;
        samplerDesc.MaxLOD = maxLod;
        memcpy(&samplerDesc.BorderColor, borderColor.Data(), 4 * sizeof(float));

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
        d3dDevice->CreateSamplerState(&samplerDesc, (ID3D11SamplerState**)&sampler);

        if (!sampler)
        {
            LOGERROR("Failed to create sampler state");
            return false;
        }
        else
            LOGDEBUG("Created sampler state");
    }

    return true;
}

void* Texture::RenderTargetViewObject(size_t index) const
{
    /// \todo Handle different indices for eg. cube maps
    return renderTargetView;
}

}
