// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLTexture.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

unsigned Texture::glTarget[] = {
    GL_TEXTURE_1D,
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_CUBE_MAP
};

Texture::Texture() :
    texture(0),
    type(TEX_2D),
    usage(USAGE_DEFAULT),
    width(0),
    height(0),
    format(FMT_NONE)
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
    bool success = Define(TEX_2D, USAGE_IMMUTABLE, image->Width(), image->Height(), image->Format(), initialData.Size(), &initialData[0]);
    /// \todo Read a parameter file for the sampling parameters
    success &= DefineSampler(FILTER_TRILINEAR, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP, 16, 0.0f, M_INFINITY, Color::BLACK);

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
    /// \todo Destroy OpenGL texture
}

bool Texture::Define(TextureType type_, ResourceUsage usage_, int width_, int height_, ImageFormat format_, size_t numLevels_, const ImageLevel* initialData)
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
        /// \todo Create OpenGL texture
        {
            width = width_;
            height = height_;
            format = format_;
            numLevels = numLevels_;

            LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", width, height, (int)format, numLevels);
        }
    }

    return true;
}

bool Texture::DefineSampler(TextureFilterMode filter, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy, float minLod, float maxLod, const Color& borderColor)
{
    PROFILE(DefineTextureSampler);

    /// \todo Apply to OpenGL texture

    return true;
}

bool Texture::SetData(size_t level, const IntRect rect, const ImageLevel& data)
{
    PROFILE(UpdateTexture);

    /// \todo Apply to OpenGL texture
    return true;
}

}
