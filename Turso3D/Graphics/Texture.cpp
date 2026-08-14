// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "GLHeaders.h"
#include "Graphics.h"
#include "Texture.h"

#include <tracy/Tracy.hpp>

static size_t activeTextureUnit = 0xffffffff;
static unsigned activeTargets[MAX_TEXTURE_UNITS];
static Texture* boundTextures[MAX_TEXTURE_UNITS];

static const GLenum glTargets[] = 
{
    GL_TEXTURE_2D,
    GL_TEXTURE_3D,
    GL_TEXTURE_CUBE_MAP
};

const unsigned Texture::glInternalFormats[] =
{
    0,
    GL_R8,
    GL_RG8,
    GL_RGBA8,
    GL_ALPHA,
#ifndef GL_ES_VERSION_3_0
    GL_R16,
    GL_RG16,
    GL_RGBA16,
#else
    0,
    0,
    0,
#endif
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
    GL_R32UI,
    GL_RG32UI,
    GL_RGBA32UI,
    GL_DEPTH_COMPONENT16,
#ifndef GL_ES_VERSION_3_0
    GL_DEPTH_COMPONENT32,
#else
    GL_DEPTH_COMPONENT32F,
#endif
    GL_DEPTH24_STENCIL8,
#ifndef GL_ES_VERSION_3_0
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
#else
    0,
    0,
    0,
    GL_COMPRESSED_RGB8_ETC2,
#endif
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
    GL_RED_INTEGER,
    GL_RG_INTEGER,
    GL_RGBA_INTEGER,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_COMPONENT,
    GL_DEPTH_STENCIL,
#ifndef GL_ES_VERSION_3_0
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
    0,
#else
    0,
    0,
    0,
    GL_COMPRESSED_RGB8_ETC2,
#endif
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
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_INT,
    GL_UNSIGNED_SHORT,
#ifndef GL_ES_VERSION_3_0
    GL_UNSIGNED_INT,
#else
    GL_FLOAT,
#endif
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
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
#ifndef GL_ES_VERSION_3_0
    GL_CLAMP_TO_BORDER,
    GL_MIRROR_CLAMP_EXT
#else
    // GLES3 fallbacks
    GL_CLAMP_TO_EDGE,
    GL_MIRRORED_REPEAT
#endif
};

Texture::Texture() :
    texture(0),
    target(0),
    type(TEX_2D),
    size(IntVector3::ZERO),
    format(FMT_NONE),
    multisample(0),
    numLevels(0)
{
}

Texture::~Texture()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (Object::Subsystem<Graphics>())
        Destroy();
}

void Texture::RegisterObject()
{
    RegisterFactory<Texture>();
}

bool Texture::BeginLoad(Stream& source)
{
    ZoneScoped;

    loadImages.clear();
    loadImages.push_back(new Image());
    if (!loadImages[0]->Load(source))
    {
        loadImages.clear();
        return false;
    }

    // If image uses unsupported format, decompress to RGBA now
    if (!FormatSupported(loadImages[0]->Format()))
    {
        Image* rgbaImage = new Image();
        rgbaImage->SetSize(loadImages[0]->Size(), FMT_RGBA8);
        loadImages[0]->DecompressLevel(rgbaImage->Data(), 0);
        loadImages[0] = rgbaImage; // This destroys the original compressed image
    }

    // Construct mip levels now if image is uncompressed
    if (!loadImages[0]->IsCompressed())
    {
        Image* mipImage = loadImages[0];

        while (mipImage->Width() > 1 || mipImage->Height() > 1)
        {
            loadImages.push_back(new Image());
            mipImage->GenerateMipImage(*loadImages.back());
            mipImage = loadImages.back();
        }
    }

    return true;
}

bool Texture::EndLoad()
{
    ZoneScoped;

    if (loadImages.empty())
        return false;

    std::vector<ImageLevel> initialData;

    for (size_t i = 0; i < loadImages.size(); ++i)
    {
        for (size_t j = 0; j < loadImages[i]->NumLevels(); ++j)
            initialData.push_back(loadImages[i]->Level(j));
    }

    Image* image = loadImages[0];
    bool success = Define(TEX_2D, image->Size(), image->Format(), 1, initialData.size(), &initialData[0]);
    /// \todo Read a parameter file for the sampling parameters
    success &= DefineSampler(FILTER_TRILINEAR, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP);

    loadImages.clear();
    return success;
}

bool Texture::Define(TextureType type_, const IntVector2& size_, ImageFormat format_, int multisample_, size_t numLevels_, const ImageLevel* initialData)
{
    return Define(type_, IntVector3(size_.x, size_.y, 1), format_, multisample_, numLevels_, initialData);
}

bool Texture::Define(TextureType type_, const IntVector3& size_, ImageFormat format_, int multisample_, size_t numLevels_, const ImageLevel* initialData)
{
    ZoneScoped;

    Destroy();

    if (!FormatSupported(format_))
    {
        LOGERRORF("Unsupported texture format %d", format_);
        return false;
    }
    if (size_.x < 1 || size_.y < 1 || size_.z < 1)
    {
        LOGERROR("Texture must not have zero or negative size");
        return false;
    }
    if (type_ == TEX_2D && size_.z != 1)
    {
        LOGERROR("2D texture must have depth of 1");
        return false;
    }
    if (type_ == TEX_CUBE && (size_.x != size_.y || size_.z != MAX_CUBE_FACES))
    {
        LOGERROR("Cube map must have square dimensions and 6 faces");
        return false;
    }

    if (numLevels_ < 1)
        numLevels_ = 1;
    if (multisample_ < 1)
        multisample_ = 1;
#ifdef GL_ES_VERSION_3_0
    if (multisample_ > 1)
    {
        LOGWARNING("Multisampled texture not supported on GLES3, falling back to non-multisampled");
        multisample_ = 1;
    }
#endif

    type = type_;

    glGenTextures(1, &texture);
    if (!texture)
    {
        size = IntVector3::ZERO;
        format = FMT_NONE;
        numLevels = 0;
        multisample = 0;

        LOGERROR("Failed to create texture");
        return false;
    }

    size = size_;
    format = format_;
    numLevels = numLevels_;
    multisample = multisample_;

    target = glTargets[type];
#ifndef GL_ES_VERSION_3_0
    if (target == GL_TEXTURE_2D && multisample > 1)
        target = GL_TEXTURE_2D_MULTISAMPLE;
#endif

    ForceBind();

    // If not compressed and no initial data, create the initial level 0 texture with null data
    // Clear previous error first to be able to check whether the data was successfully set
    glGetError();
    if (!IsCompressed() && !initialData)
    {
        if (multisample == 1)
        {
            if (type == TEX_2D)
                glTexImage2D(target, 0, glInternalFormats[format], size.x, size.y, 0, glFormats[format], glDataTypes[format], nullptr);
            else if (type == TEX_3D)
                glTexImage3D(target, 0, glInternalFormats[format], size.x, size.y, size.z, 0, glFormats[format], glDataTypes[format], nullptr);
            else if (type == TEX_CUBE)
            {
                for (size_t i = 0; i < MAX_CUBE_FACES; ++i)
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, 0, glInternalFormats[format], size.x, size.y, 0, glFormats[format], glDataTypes[format], nullptr);
            }
        }
#ifndef GL_ES_VERSION_3_0
        else
        {
            if (type == TEX_2D)
                glTexImage2DMultisample(target, multisample, glInternalFormats[format], size.x, size.y, GL_TRUE);
            else if (type == TEX_3D)
                glTexImage3DMultisample(target, multisample, glInternalFormats[format], size.x, size.y, size.z, GL_TRUE);
            else if (type == TEX_CUBE)
            {
                for (size_t i = 0; i < MAX_CUBE_FACES; ++i)
                    glTexImage2DMultisample(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, multisample, glInternalFormats[format], size.x, size.y, GL_TRUE);
            }
        }
#endif
    }

    if (initialData)
    {
        for (size_t i = 0; i < numLevels; ++i)
        {
            if (type != TEX_3D)
            {
                for (int j = 0; j < size.z; ++j)
                    SetData(i, IntBox(0, 0, j, Max(size.x >> i, 1), Max(size.y >> i, 1), j+1), initialData[i * size.z + j]);
            }
            else
                SetData(i, IntBox(0, 0, 0, Max(size.x >> i, 1), Max(size.y >> i, 1), Max(size.z >> i, 1)), initialData[i]);
        }
    }

    // If we have an error now, the texture was not created correctly
    if (glGetError() != GL_NO_ERROR)
    {
        Destroy();
        LOGERROR("Failed to create texture");
        return false;
    }

    glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, type != TEX_3D ? (unsigned)numLevels - 1 : 0);
    LOGDEBUGF("Created texture width %d height %d depth %d format %d numLevels %d", size.x, size.y, size.z, (int)format, numLevels);

    return true;
}

bool Texture::DefineSampler(TextureFilterMode filter_, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy_, float minLod_, float maxLod_, const Color& borderColor_)
{
    ZoneScoped;

    filter = filter_;
    addressModes[0] = u;
    addressModes[1] = v;
    addressModes[2] = w;
    maxAnisotropy = maxAnisotropy_;
    minLod = minLod_;
    maxLod = maxLod_;
    borderColor = borderColor_;

    if (!texture)
    {
        LOGERROR("Texture must be defined before defining sampling parameters");
        return false;
    }

    ForceBind();

    switch (filter)
    {
    case FILTER_POINT:
    case COMPARE_POINT:
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        break;

    case FILTER_BILINEAR:
    case COMPARE_BILINEAR:
        if (numLevels < 2)
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        else
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;

    case FILTER_ANISOTROPIC:
    case FILTER_TRILINEAR:
    case COMPARE_ANISOTROPIC:
    case COMPARE_TRILINEAR:
        if (numLevels < 2)
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        else
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;

    default:
        break;
    }

    glTexParameteri(target, GL_TEXTURE_WRAP_S, glWrapModes[addressModes[0]]);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, glWrapModes[addressModes[1]]);
    glTexParameteri(target, GL_TEXTURE_WRAP_R, glWrapModes[addressModes[2]]);

    glTexParameterf(target, GL_TEXTURE_MIN_LOD, minLod);
    glTexParameterf(target, GL_TEXTURE_MAX_LOD, maxLod);

#ifndef GL_ES_VERSION_3_0
    glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, filter == FILTER_ANISOTROPIC ? maxAnisotropy : 1.0f);
    glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor.Data());
#endif        

    if (filter >= COMPARE_POINT)
    {
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }
    else
        glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    return true;
}

bool Texture::SetData(size_t level, const IntRect& rect, const ImageLevel& data)
{
    return SetData(level, IntBox(rect.left, rect.top, 0, rect.right, rect.bottom, 1), data);
}

bool Texture::SetData(size_t level, const IntBox& box, const ImageLevel& data)
{
    if (!texture)
        return true;

    if (multisample > 1)
    {
        LOGERROR("Cannot set data on multisampled texture");
        return false;
    }

    if (level >= numLevels)
    {
        LOGERROR("Mipmap level to update out of bounds");
        return false;
    }

    GLenum glTarget = (type == TEX_CUBE) ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + box.near : target;

    IntBox levelBox(0, 0, 0, Max(size.x >> level, 1), Max(size.y >> level, 1), Max(size.z >> level, 1));
    if (type == TEX_CUBE)
    {
        if (box.Depth() != 1)
        {
            LOGERROR("Cube map must update one face at a time");
            return false;
        }

        levelBox.near = box.near;
        levelBox.far = box.far;
    }

    if (levelBox.IsInside(box) != INSIDE)
    {
        LOGERRORF("Texture update region %s is outside level %s", box.ToString().c_str(), levelBox.ToString().c_str());
        return false;
    }

    ForceBind();

    bool wholeLevel = box == levelBox;

    if (type != TEX_3D)
    {
        if (!IsCompressed())
        {
            if (wholeLevel)
                glTexImage2D(glTarget, (int)level, glInternalFormats[format], box.Width(), box.Height(), 0, glFormats[format], glDataTypes[format], data.data);
            else
                glTexSubImage2D(glTarget, (int)level, box.left, box.top, box.Width(), box.Height(), glFormats[format], glDataTypes[format], data.data);
        }
        else
        {
            if (wholeLevel)
                glCompressedTexImage2D(glTarget, (int)level, glInternalFormats[format], box.Width(), box.Height(), 0, (GLsizei)data.dataSize, data.data);
            else
                glCompressedTexSubImage2D(glTarget, (int)level, box.left, box.top, box.Width(), box.Height(), glFormats[format], (GLsizei)data.dataSize, data.data);
        }
    }
    else
    {
        if (wholeLevel)
            glTexImage3D(glTarget, (int)level, glInternalFormats[format], box.Width(), box.Height(), box.Depth(), 0, glFormats[format], glDataTypes[format], data.data);
        else
            glTexSubImage3D(glTarget, (int)level, box.left, box.top, box.near, box.Width(), box.Height(), box.Depth(), glFormats[format], glDataTypes[format], data.data);
    }

    return true;
}

void Texture::Bind(size_t unit)
{
    if (unit >= MAX_TEXTURE_UNITS || !texture || boundTextures[unit] == this)
        return;

    if (activeTextureUnit != unit)
    {
        glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
        activeTextureUnit = unit;
    }

    if (activeTargets[unit] && activeTargets[unit] != target)
        glBindTexture(activeTargets[unit], 0);

    glBindTexture(target, texture);
    activeTargets[unit] = target;
    boundTextures[unit] = this;
}

void Texture::Destroy()
{
    if (texture)
    {
        glDeleteTextures(1, &texture);
        texture = 0;
        size = IntVector3::ZERO;
        format = FMT_NONE;
        numLevels = 0;

        for (size_t i = 0; i < MAX_TEXTURE_UNITS; ++i)
        {
            if (boundTextures[i] == this)
                boundTextures[i] = nullptr;
        }
    }
}

void Texture::Unbind(size_t unit)
{
    if (boundTextures[unit])
    {
        if (activeTextureUnit != unit)
        {
            glActiveTexture(GL_TEXTURE0 + (GLenum)unit);
            activeTextureUnit = unit;
        }
        glBindTexture(activeTargets[unit], 0);
        activeTargets[unit] = 0;
        boundTextures[unit] = nullptr;
    }
}

bool Texture::FormatSupported(ImageFormat format)
{
    return glInternalFormats[format] != 0;
}

unsigned Texture::GLTarget() const
{
    return target;
}

void Texture::ForceBind()
{
    boundTextures[0] = nullptr;
    Bind(0);
}
