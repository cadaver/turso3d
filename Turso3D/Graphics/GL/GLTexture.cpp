// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLTexture.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

static const unsigned glTargets[] = 
{
    GL_TEXTURE_1D,
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_CUBE_MAP
};

static const unsigned glInternalFormats[] =
{
    0,
    GL_R8,
    GL_RG8,
    GL_RGBA8,
    GL_ALPHA,
    GL_R16,
    GL_RG16,
    GL_RGBA16,
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT32,
    GL_DEPTH24_STENCIL8,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
    0,
    0,
    0,
    0
};

static const unsigned glFormats[] =
{
    0,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_ALPHA,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
    0,
    0,
    0,
    0
};

static const unsigned glDataTypes[] = 
{
    0,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_SHORT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_UNSIGNED_SHORT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT_24_8,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static const unsigned glWrapModes[] = 
{
    0,
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
    GL_CLAMP_TO_BORDER,
    GL_MIRROR_CLAMP_EXT
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

            // Clear from all FBO's
            graphics->CleanupFramebuffers(this);
        }
    }

    if (texture)
    {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
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
        glGenTextures(1, &texture);
        if (!texture)
        {
            width = 0;
            height = 0;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }

        // Ensure the texture is bound for creation
        graphics->SetTexture(0, this);

        width = width_;
        height = height_;
        format = format_;
        numLevels = numLevels_;

        // If not compressed and no initial data, create the initial level 0 texture with null data
        if (!IsCompressed() && !initialData)
            glTexImage2D(glTargets[type], 0, glInternalFormats[format], width, height, 0, glFormats[format], glDataTypes[format], 0);

        if (initialData)
        {
            // Hack for allowing immutable texture to set initial data
            usage = USAGE_DEFAULT;
            for (size_t i = 0; i < numLevels; ++i)
                SetData(i, IntRect(0, 0, Max(width >> i, 1), Max(height >> i, 1)), initialData[i]);
            usage = usage_;
        }

        glTexParameteri(glTargets[type], GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(glTargets[type], GL_TEXTURE_MAX_LEVEL, (unsigned)numLevels - 1);

        LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", width, height, (int)format, numLevels);
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

    if (graphics && graphics->IsInitialized())
    {
        if (!texture)
        {
            LOGERROR("On OpenGL texture must be defined before defining sampling parameters");
            return false;
        }

        // Bind for defining sampling parameters
        graphics->SetTexture(0, this);

        switch (filter)
        {
        case FILTER_POINT:
            glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(glTargets[type], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;

        case FILTER_BILINEAR:
            if (numLevels < 2)
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            else
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glTexParameteri(glTargets[type], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;

        case FILTER_ANISOTROPIC:
        case FILTER_TRILINEAR:
            if (numLevels < 2)
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            else
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(glTargets[type], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;

        default:
            break;
        }

        glTexParameteri(glTargets[type], GL_TEXTURE_WRAP_S, glWrapModes[addressModes[0]]);
        glTexParameteri(glTargets[type], GL_TEXTURE_WRAP_T, glWrapModes[addressModes[1]]);
        glTexParameteri(glTargets[type], GL_TEXTURE_WRAP_R, glWrapModes[addressModes[2]]);

        glTexParameterf(glTargets[type], GL_TEXTURE_MAX_ANISOTROPY_EXT, filter == FILTER_ANISOTROPIC ?
            maxAnisotropy : 1.0f);

        glTexParameterf(glTargets[type], GL_TEXTURE_MIN_LOD, minLod);
        glTexParameterf(glTargets[type], GL_TEXTURE_MAX_LOD, maxLod);

        glTexParameterfv(glTargets[type], GL_TEXTURE_BORDER_COLOR, borderColor.Data());
    }

    return true;
}

bool Texture::SetData(size_t level, const IntRect rect, const ImageLevel& data)
{
    PROFILE(UpdateTextureLevel);

    if (texture)
    {
        if (usage == USAGE_IMMUTABLE)
        {
            LOGERROR("Can not update immutable texture");
            return false;
        }
        if (level >= numLevels)
        {
            LOGERROR("Mipmap level to update out of bounds");
            return false;
        }
        
        IntRect levelRect(0, 0, Max(width >> level, 1), Max(height >> level, 1));
        if (levelRect.IsInside(rect) != INSIDE)
        {
            LOGERRORF("Texture update region %s is outside level %s", rect.ToString().CString(), levelRect.ToString().CString());
            return false;
        }

        // Bind for updating
        graphics->SetTexture(0, this);

        bool wholeLevel = rect == levelRect;
        if (!IsCompressed())
        {
            if (wholeLevel)
            {
                glTexImage2D(glTargets[type], (unsigned)level, glInternalFormats[format], rect.Width(), rect.Height(), 0,
                    glFormats[format], glDataTypes[format], data.data);
            }
            else
            {
                glTexSubImage2D(glTargets[type], (unsigned)level, rect.left, rect.top, rect.Width(), rect.Height(), 
                    glFormats[format], glDataTypes[format], data.data);
            }
        }
        else
        {
            if (wholeLevel)
            {
                glCompressedTexImage2D(glTargets[type], (unsigned)level, glInternalFormats[format], rect.Width(), rect.Height(),
                    0, (unsigned)Image::CalculateDataSize(rect.Width(), rect.Height(), format), data.data);
            }
            else
            {
                glCompressedTexSubImage2D(glTargets[type], (unsigned)level, rect.left, rect.top, rect.Width(), rect.Height(),
                    glFormats[format], (unsigned)Image::CalculateDataSize(rect.Width(), rect.Height(), format), data.data);
            }
        }
    }

    return true;
}

unsigned Texture::GLTarget() const
{
    return glTargets[type];
}

}
