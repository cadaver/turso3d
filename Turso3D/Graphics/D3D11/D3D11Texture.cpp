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
    D3D11_FILTER_ANISOTROPIC,
    D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT,
    D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
    D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
    D3D11_FILTER_COMPARISON_ANISOTROPIC
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
    DXGI_FORMAT_R16_TYPELESS,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_R24G8_TYPELESS,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC3_UNORM
};

static const DXGI_FORMAT depthStencilViewFormat[] =
{
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_D32_FLOAT,
    DXGI_FORMAT_D24_UNORM_S8_UINT
};

static const DXGI_FORMAT depthStencilResourceViewFormat[] =
{
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS
};

static const D3D11_SRV_DIMENSION srvDimension[] = 
{
    D3D11_SRV_DIMENSION_TEXTURE1D,
    D3D11_SRV_DIMENSION_TEXTURE2D,
    D3D11_SRV_DIMENSION_TEXTURE3D,
    D3D11_SRV_DIMENSION_TEXTURECUBE
};

static const D3D11_RTV_DIMENSION rtvDimension[] = 
{
    D3D11_RTV_DIMENSION_TEXTURE1D,
    D3D11_RTV_DIMENSION_TEXTURE2D,
    D3D11_RTV_DIMENSION_TEXTURE3D,
    D3D11_RTV_DIMENSION_TEXTURE2D, /// \todo Implement views per face
};

static const D3D11_DSV_DIMENSION dsvDimension[] =
{
    D3D11_DSV_DIMENSION_TEXTURE1D,
    D3D11_DSV_DIMENSION_TEXTURE2D,
    D3D11_DSV_DIMENSION_TEXTURE2D,
    D3D11_DSV_DIMENSION_TEXTURE2D,
};

Texture::Texture() :
    texture(nullptr),
    resourceView(nullptr),
    sampler(nullptr),
    type(TEX_2D),
    usage(USAGE_DEFAULT),
    size(IntVector2::ZERO),
    format(FMT_NONE)
{
}

Texture::~Texture()
{
    Release();
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
        resourceView = nullptr;
    }
    if (renderTargetView)
    {
        if (IsRenderTarget())
        {
            ID3D11RenderTargetView* d3dRenderTargetView = (ID3D11RenderTargetView*)renderTargetView;
            d3dRenderTargetView->Release();
        }
        else if (IsDepthStencil())
        {
            ID3D11DepthStencilView* d3dDepthStencilView = (ID3D11DepthStencilView*)renderTargetView;
            d3dDepthStencilView->Release();
        }
        renderTargetView = nullptr;
    }
    if (sampler)
    {
        ID3D11SamplerState* d3dSampler = (ID3D11SamplerState*)sampler;
        d3dSampler->Release();
        sampler = nullptr;
    }
    if (texture)
    {
        ID3D11Resource* d3dTexture = (ID3D11Resource*)texture;
        d3dTexture->Release();
        texture = nullptr;
    }
}

bool Texture::Define(TextureType type_, ResourceUsage usage_, const IntVector2& size_, ImageFormat format_, size_t numLevels_, const ImageLevel* initialData)
{
    PROFILE(DefineTexture);

    Release();

    if (type_ != TEX_2D && type_ != TEX_CUBE)
    {
        LOGERROR("Only 2D textures and cube maps supported for now");
        return false;
    }
    if (format_ > FMT_DXT5)
    {
        LOGERROR("ETC1 and PVRTC formats are unsupported");
        return false;
    }
    if (type_ == TEX_CUBE && size_.x != size_.y)
    {
        LOGERROR("Cube map must have square dimensions");
        return false;
    }

    if (numLevels_ < 1)
        numLevels_ = 1;

    type = type_;
    usage = usage_;

    if (graphics && graphics->IsInitialized())
    {
        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->D3DDevice();

        D3D11_TEXTURE2D_DESC textureDesc;
        memset(&textureDesc, 0, sizeof textureDesc);
        textureDesc.Width = size_.x;
        textureDesc.Height = size_.y;
        textureDesc.MipLevels = (unsigned)numLevels_;
        textureDesc.ArraySize = NumFaces();
        textureDesc.Format = textureFormat[format_];
        /// \todo Support defining multisampled textures
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = (usage != USAGE_RENDERTARGET) ? (D3D11_USAGE)usage : D3D11_USAGE_DEFAULT;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (usage == USAGE_RENDERTARGET)
        {
            if (format_ < FMT_D16 || format_ > FMT_D24S8)
                textureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            else
                textureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        }
        textureDesc.CPUAccessFlags = usage == USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;
        if (type == TEX_CUBE)
            textureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        Vector<D3D11_SUBRESOURCE_DATA> subResourceData;
        if (initialData)
        {
            subResourceData.Resize(NumFaces() * numLevels_);
            for (size_t i = 0; i < NumFaces() * numLevels_; ++i)
            {
                subResourceData[i].pSysMem = initialData[i].data;
                subResourceData[i].SysMemPitch = (unsigned)initialData[i].rowSize;
                subResourceData[i].SysMemSlicePitch = 0;
            }
        }

        d3dDevice->CreateTexture2D(&textureDesc, subResourceData.Size() ? &subResourceData[0] : (D3D11_SUBRESOURCE_DATA*)0, (ID3D11Texture2D**)&texture);
        if (!texture)
        {
            size = IntVector2::ZERO;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }
        else
        {
            size = size_;
            format = format_;
            numLevels = numLevels_;

            LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", size.x, size.y, (int)format, numLevels);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
        memset(&resourceViewDesc, 0, sizeof resourceViewDesc);
        resourceViewDesc.ViewDimension = srvDimension[type];
        switch (type)
        {
        case TEX_2D:
            resourceViewDesc.Texture2D.MipLevels = (unsigned)numLevels;
            resourceViewDesc.Texture2D.MostDetailedMip = 0;
            break;

        case TEX_CUBE:
            resourceViewDesc.TextureCube.MipLevels = (unsigned)numLevels;
            resourceViewDesc.TextureCube.MostDetailedMip = 0;
            break;
        }
        resourceViewDesc.Format = textureDesc.Format;

        if (IsRenderTarget())
        {
            D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
            memset(&renderTargetViewDesc, 0, sizeof renderTargetViewDesc);
            renderTargetViewDesc.Format = textureDesc.Format;
            renderTargetViewDesc.ViewDimension = rtvDimension[type];
            renderTargetViewDesc.Texture2D.MipSlice = 0;

            d3dDevice->CreateRenderTargetView((ID3D11Resource*)texture, &renderTargetViewDesc, (ID3D11RenderTargetView**)&renderTargetView);
            if (!renderTargetView)
            {
                LOGERROR("Failed to create rendertarget view for texture");
            }
        }
        else if (IsDepthStencil())
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
            memset(&depthStencilViewDesc, 0, sizeof depthStencilViewDesc);
            // Readable depth textures are created typeless, while the actual format is specified for the depth stencil
            // and shader resource views
            resourceViewDesc.Format = depthStencilResourceViewFormat[format - FMT_D16];
            depthStencilViewDesc.Format = depthStencilViewFormat[format - FMT_D16];
            depthStencilViewDesc.ViewDimension = dsvDimension[type];
            depthStencilViewDesc.Flags = 0;

            d3dDevice->CreateDepthStencilView((ID3D11Resource*)texture, &depthStencilViewDesc, (ID3D11DepthStencilView**)&renderTargetView);
            if (!renderTargetView)
            {
                LOGERROR("Failed to create depth-stencil view for texture");
            }
        }

        d3dDevice->CreateShaderResourceView((ID3D11Resource*)texture, &resourceViewDesc, (ID3D11ShaderResourceView**)&resourceView);
        if (!resourceView)
        {
            LOGERROR("Failed to create shader resource view for texture");
        }
    }

    return true;
}

bool Texture::DefineSampler(TextureFilterMode filter_, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy_, float minLod_, float maxLod_, const Color& borderColor_)
{
    PROFILE(DefineTextureSampler);

    filter = filter_;
    addressModes[0] = u;
    addressModes[1] = v;
    addressModes[2] = w;
    maxAnisotropy = maxAnisotropy_;
    minLod = minLod_;
    maxLod = maxLod_;
    borderColor = borderColor_;
    
    // Release the previous sampler first
    if (sampler)
    {
        ID3D11SamplerState* d3dSampler = (ID3D11SamplerState*)sampler;
        d3dSampler->Release();
        sampler = nullptr;
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
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        samplerDesc.MinLOD = minLod;
        samplerDesc.MaxLOD = maxLod;
        memcpy(&samplerDesc.BorderColor, borderColor.Data(), 4 * sizeof(float));

        ID3D11Device* d3dDevice = (ID3D11Device*)graphics->D3DDevice();
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

bool Texture::SetData(size_t face, size_t level, const IntRect rect, const ImageLevel& data)
{
    PROFILE(UpdateTextureLevel);

    if (texture)
    {
        if (usage == USAGE_IMMUTABLE)
        {
            LOGERROR("Can not update immutable texture");
            return false;
        }
        if (face >= NumFaces())
        {
            LOGERROR("Face to update out of bounds");
            return false;
        }
        if (level >= numLevels)
        {
            LOGERROR("Mipmap level to update out of bounds");
            return false;
        }

        IntRect levelRect(0, 0, Max(size.x >> level, 1), Max(size.y >> level, 1));
        if (levelRect.IsInside(rect) != INSIDE)
        {
            LOGERROR("Texture update region is outside level");
            return false;
        }

        ID3D11DeviceContext* d3dDeviceContext = (ID3D11DeviceContext*)graphics->D3DDeviceContext();
        unsigned subResource = D3D11CalcSubresource(level, face, numLevels);

        if (usage == USAGE_DYNAMIC)
        {
            size_t pixelByteSize = Image::pixelByteSizes[format];
            if (!pixelByteSize)
            {
                LOGERROR("Updating dynamic compressed texture is not supported");
                return false;
            }

            D3D11_MAPPED_SUBRESOURCE mappedData;
            mappedData.pData = nullptr;

            d3dDeviceContext->Map((ID3D11Resource*)texture, subResource, D3D11_MAP_WRITE_DISCARD, 0, &mappedData);
            if (mappedData.pData)
            {
                for (int y = rect.top; y < rect.bottom; ++y)
                {
                    memcpy((unsigned char*)mappedData.pData + y * mappedData.RowPitch + rect.left + pixelByteSize, data.data +
                        (y - rect.top) * data.rowSize, (rect.right - rect.left) * pixelByteSize);
                }
            }
            else
            {
                LOGERROR("Failed to map texture for update");
                return false;
            }
        }
        else
        {
            D3D11_BOX destBox;
            destBox.left = rect.left;
            destBox.right = rect.right;
            destBox.top = rect.top;
            destBox.bottom = rect.bottom;
            destBox.front = 0;
            destBox.back = 1;

            d3dDeviceContext->UpdateSubresource((ID3D11Resource*)texture, subResource, &destBox, data.data,
                (unsigned)data.rowSize, 0);
        }
    }

    return true;
}

void* Texture::D3DRenderTargetView(size_t /*index*/) const
{
    /// \todo Handle different indices for eg. cube maps
    return renderTargetView;
}

}
