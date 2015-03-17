// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Resource/ResourceCache.h"
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

void Texture::Recreate()
{
    // If has a name, attempt to reload through the resource cache
    if (Name().Length())
    {
        ResourceCache* cache = Subsystem<ResourceCache>();
        if (cache && cache->ReloadResource(this))
            return;
    }

    // If failed to reload, recreate the texture without data and mark data lost
    Define(type, usage, size, format, numLevels);
    SetDataLost(true);
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
        glGenTextures(1, &texture);
        if (!texture)
        {
            size = IntVector2::ZERO;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }

        // Ensure the texture is bound for creation
        graphics->SetTexture(0, this);

        size = size_;
        format = format_;
        numLevels = numLevels_;

        // If not compressed and no initial data, create the initial level 0 texture with null data
        // Clear previous error first to be able to check whether the data was successfully set
        glGetError();
        if (!IsCompressed() && !initialData)
        {
            if (type == TEX_2D)
                glTexImage2D(glTargets[type], 0, glInternalFormats[format], size.x, size.y, 0, glFormats[format], glDataTypes[format], 0);
            else if (type == TEX_CUBE)
            {
                for (size_t i = 0; i < MAX_CUBE_FACES; ++i)
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, glInternalFormats[format], size.x, size.y, 0, glFormats[format], glDataTypes[format], 0);
            }
        }

        if (initialData)
        {
            // Hack for allowing immutable texture to set initial data
            usage = USAGE_DEFAULT;
            size_t idx = 0;
            for (size_t i = 0; i < NumFaces(); ++i)
            {
                for (size_t j = 0; j < numLevels; ++j)
                    SetData(i, j, IntRect(0, 0, Max(size.x >> j, 1), Max(size.y >> j, 1)), initialData[idx++]);
            }
            usage = usage_;
        }

        // If we have an error now, the texture was not created correctly
        if (glGetError() != GL_NO_ERROR)
        {
            Release();
            size = IntVector2::ZERO;
            format = FMT_NONE;
            numLevels = 0;

            LOGERROR("Failed to create texture");
            return false;
        }

        glTexParameteri(glTargets[type], GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(glTargets[type], GL_TEXTURE_MAX_LEVEL, (unsigned)numLevels - 1);
        LOGDEBUGF("Created texture width %d height %d format %d numLevels %d", size.x, size.y, (int)format, numLevels);
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
        case COMPARE_POINT:
            glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(glTargets[type], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;

        case FILTER_BILINEAR:
        case COMPARE_BILINEAR:
            if (numLevels < 2)
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            else
                glTexParameteri(glTargets[type], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
            glTexParameteri(glTargets[type], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;

        case FILTER_ANISOTROPIC:
        case FILTER_TRILINEAR:
        case COMPARE_ANISOTROPIC:
        case COMPARE_TRILINEAR:
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
        
        if (filter >= COMPARE_POINT)
        {
            glTexParameteri(glTargets[type], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glTexParameteri(glTargets[type], GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        }
        else
            glTexParameteri(glTargets[type], GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

    return true;
}

bool Texture::SetData(size_t face, size_t level, IntRect rect, const ImageLevel& data)
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
            LOGERRORF("Texture update region %s is outside level %s", rect.ToString().CString(), levelRect.ToString().CString());
            return false;
        }

        // Bind for updating
        graphics->SetTexture(0, this);

        bool wholeLevel = rect == levelRect;
        unsigned target = (type == TEX_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face: glTargets[type];
        
        if (!IsCompressed())
        {
            if (wholeLevel)
            {
                glTexImage2D(target, (unsigned)level, glInternalFormats[format], rect.Width(), rect.Height(), 0,
                    glFormats[format], glDataTypes[format], data.data);
            }
            else
            {
                glTexSubImage2D(target, (unsigned)level, rect.left, rect.top, rect.Width(), rect.Height(), 
                    glFormats[format], glDataTypes[format], data.data);
            }
        }
        else
        {
            if (wholeLevel)
            {
                glCompressedTexImage2D(target, (unsigned)level, glInternalFormats[format], rect.Width(), rect.Height(),
                    0, (unsigned)Image::CalculateDataSize(IntVector2(rect.Width(), rect.Height()), format), data.data);
            }
            else
            {
                glCompressedTexSubImage2D(target, (unsigned)level, rect.left, rect.top, rect.Width(), rect.Height(),
                    glFormats[format], (unsigned)Image::CalculateDataSize(IntVector2(rect.Width(), rect.Height()), format),
                    data.data);
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
