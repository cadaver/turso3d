// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "Resource.h"

struct SDL_Surface;

namespace Turso3D
{

/// Image formats.
enum ImageFormat
{
    FMT_NONE = 0,
    FMT_L8,
    FMT_LA8,
    FMT_RGB8,
    FMT_RGBA8,
    FMT_A8,
    FMT_R16,
    FMT_RG16,
    FMT_RGB16,
    FMT_RGBA16,
    FMT_R32,
    FMT_RG32,
    FMT_RGB32,
    FMT_RGBA32,
    FMT_R16F,
    FMT_RG16F,
    FMT_RGB16F,
    FMT_RGBA16F,
    FMT_R32F,
    FMT_RG32F,
    FMT_RGB32F,
    FMT_RGBA32F,
    FMT_D16,
    FMT_D24,
    FMT_D32,
    FMT_D24S8,
    FMT_DXT1,
    FMT_DXT3,
    FMT_DXT5,
    FMT_ETC1,
    FMT_PVRTC_RGB_2BPP,
    FMT_PVRTC_RGBA_2BPP,
    FMT_PVRTC_RGB_4BPP,
    FMT_PVRTC_RGBA_4BPP,
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
    ImageFormat format;
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

    /// Set new image pixel dimensions and format. Setting a compressed format is not supported.
    void SetSize(int newWidth, int newHeight, ImageFormat newFormat);
    /// Set new pixel data.
    void SetData(const unsigned char* pixelData);

    /// Return image width in pixels.
    int Width() const { return width; }
    /// Return image height in pixels.
    int Height() const { return height; }
    /// Return byte size of a pixel. Will return 0 for compressed formats.
    size_t PixelByteSize() const { return pixelByteSize[format]; } 
    /// Return pixel data.
    unsigned char* Data() const { return data; }
    /// Return whether is a compressed image.
    bool IsCompressed() const { return format >= FMT_DXT1; }
    /// Return the image format.
    ImageFormat Format() const { return format; }
    /// Return number of mipmaps if compressed.
    size_t NumCompressedLevels() const { return numCompressedLevels; }
    /// Calculate the next mip level. Supports uncompressed images only. Return true on success.
    bool GenerateNextMipLevel(Image& dest) const;
    /// Return a compressed mip level. Supports compressed images only.
    CompressedLevel CompressedMipLevel(size_t index) const;

    /// Pixel byte sizes per format.
    static const size_t pixelByteSize[];

private:
    /// Decode image pixel data using the stb_image library.
    static unsigned char* DecodePixelData(Stream& source, int& width, int& height, unsigned& components);
    /// Free the decoded pixel data.
    static void FreePixelData(unsigned char* pixelData);

    /// Image width in pixels.
    int width;
    /// Image height in pixels.
    int height;
    /// Image format.
    ImageFormat format;
    /// Number of compressed mip levels.
    size_t numCompressedLevels;
    /// Image pixel data.
    AutoArrayPtr<unsigned char> data;
};

}
