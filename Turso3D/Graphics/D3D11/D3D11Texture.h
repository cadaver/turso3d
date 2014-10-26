// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Math/Color.h"
#include "../../Resource/Image.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

class Image;

/// %Texture mip level data definition.
struct TextureData
{
    /// Pointer to pixel data.
    void* data;
    /// Row pitch.
    size_t pitch;
};

/// %Texture on the GPU.
class Texture : public Resource, public GPUObject
{
    OBJECT(Texture);

public:
    /// Construct.
    Texture();
    /// Destruct.
    virtual ~Texture();

    /// Register object factory.
    static void RegisterObject();

    /// Load the texture image data from a stream. Return true on success.
    virtual bool BeginLoad(Stream& source);
    /// Finish texture loading by uploading to the GPU. Return true on success.
    virtual bool EndLoad();
    /// Release the texture and sampler objects.
    virtual void Release();

    /// Define texture type and dimensions and set initial data. Return true on success.
    bool Define(TextureType type, TextureUsage usage, int width, int height, ImageFormat format, size_t numLevels, TextureData* initialData = 0);
    /// Define sampling parameters. Return true on success.
    bool DefineSampler(TextureFilterMode filter, TextureAddressMode u, TextureAddressMode v, TextureAddressMode w, unsigned maxAnisotropy, float minLod, float maxLod, const Color& borderColor);

    /// Return the D3D11 texture object.
    void* TextureObject() const { return texture; }
    /// Return the D3D11 shader resource view object.
    void* ResourceViewObject() const { return resourceView; }
    /// Return the D3D11 texture sampler object.
    void* SamplerObject() const { return sampler; }
    /// Return texture type.
    TextureType TexType() const { return type; }
    /// Return width.
    int Width() const { return width; }
    /// Return height.
    int Height() const { return height; }
    /// Return image format.
    ImageFormat Format() const { return format; }
    /// Return number of mipmap levels.
    size_t NumLevels() const { return numLevels; }
    /// Return usage mode.
    TextureUsage Usage() const { return usage; }
    
private: 
    /// D3D11 texture object.
    void* texture;
    /// D3D11 resource view object.
    void* resourceView;
    /// D3D11 texture sampler object.
    void* sampler;
    /// Texture type.
    TextureType type;
    /// Texture usage mode.
    TextureUsage usage;
    /// Texture width in pixels.
    int width;
    /// Texture height in pixels.
    int height;
    /// Image format.
    ImageFormat format;
    /// Number of mipmap levels.
    size_t numLevels;
    /// Images used for loading.
    Vector<Image*> loadImages;
};

}
