// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "Resource.h"

struct SDL_Surface;

namespace Turso3D
{

/// Supported compressed image formats.
enum CompressedFormat
{
    CF_NONE = 0,
    CF_DXT1,
    CF_DXT3,
    CF_DXT5,
    CF_ETC1,
    CF_PVRTC_RGB_2BPP,
    CF_PVRTC_RGBA_2BPP,
    CF_PVRTC_RGB_4BPP,
    CF_PVRTC_RGBA_4BPP,
};

/// Compressed image mip level.
struct TURSO3D_API CompressedLevel
{
    CompressedLevel() :
        data(0),
        width(0),
        height(0),
        blockSize(0),
        dataSize(0),
        rowSize(0),
        rows(0)
    {
    }

    bool Decompress(unsigned char* dest);

    unsigned char* data;
    CompressedFormat format;
    int width;
    int height;
    size_t blockSize;
    size_t dataSize;
    size_t rowSize;
    size_t rows;
};

/// %Image resource.
class TURSO3D_API Image : public Resource
{
    OBJECT(Image);

public:
    /// Construct.
    Image();
    /// Destruct.
    virtual ~Image();

    /// Register object factory.
    static void RegisterObject();

    /// Load image from a stream. Return true on success.
    virtual bool BeginLoad(Stream& source);
    /// Save the image to a stream. Regardless of original format, the image is saved as png. Compressed image data is not supported. Return true on success.
    virtual bool Save(Stream& dest) const;

    /// Set new image pixel dimensions and number of components.
    void SetSize(int newWidth, int newHeight, unsigned newComponents);
    /// Set new pixel data.
    void SetData(const unsigned char* pixelData);

    /// Return image width in pixels.
    int Width() const { return width; }
    /// Return image height in pixels.
    int Height() const { return height; }
    /// Return number of components. RGB = 3, RGBA = 4.
    unsigned Components() const { return components; }
    /// Return pixel data.
    unsigned char* Data() const { return data; }
    /// Return whether is a compressed image.
    bool IsCompressed() const { return compressedFormat != CF_NONE; }
    /// Return the compressed format.
    CompressedFormat Format() const { return compressedFormat; }
    /// Return number of mipmaps if compressed.
    size_t NumCompressedLevels() const { return numCompressedLevels; }
    /// Calculate the next mip level (uncompressed images only.)
    AutoPtr<Image> GenerateNextMipLevel() const;
    /// Return a compressed mip level (compressed images only.)
    CompressedLevel CompressedMipLevel(size_t index) const;

private:
    /// Decode image pixel data using the stb_image library.
    static unsigned char* DecodePixelData(Stream& source, int& width, int& height, unsigned& components);
    /// Free the decoded pixel data.
    static void FreePixelData(unsigned char* pixelData);

    /// Image width in pixels.
    int width;
    /// Image height in pixels.
    int height;
    /// Number of components.
    unsigned components;
    /// Number of compressed mip levels.
    size_t numCompressedLevels;
    /// Compressed data format, or CF_NONE if not compressed
    CompressedFormat compressedFormat;
    /// Image pixel data.
    AutoArrayPtr<unsigned char> data;
};

}
