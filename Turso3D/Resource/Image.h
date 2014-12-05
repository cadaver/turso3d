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
    FMT_R8,
    FMT_RG8,
    FMT_RGBA8,
    FMT_A8,
    FMT_R16,
    FMT_RG16,
    FMT_RGBA16,
    FMT_R16F,
    FMT_RG16F,
    FMT_RGBA16F,
    FMT_R32F,
    FMT_RG32F,
    FMT_RGB32F,
    FMT_RGBA32F,
    FMT_D16,
    FMT_D32,
    FMT_D24S8,
    FMT_DXT1,
    FMT_DXT3,
    FMT_DXT5,
    FMT_ETC1,
    FMT_PVRTC_RGB_2BPP,
    FMT_PVRTC_RGBA_2BPP,
    FMT_PVRTC_RGB_4BPP,
    FMT_PVRTC_RGBA_4BPP
};

/// Description of image mip level data.
struct TURSO3D_API ImageLevel
{
    /// Default construct.
    ImageLevel() :
        data(nullptr),
        width(0),
        height(0),
        rowSize(0),
        rows(0)
    {
    }

    /// Pointer to pixel data.
    unsigned char* data;
    /// Level width.
    int width;
    /// Level height.
    int height;
    /// Row size in bytes.
    size_t rowSize;
    /// Number of rows.
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
    ~Image();

    /// Register object factory.
    static void RegisterObject();

    /// Load image from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    /// Save the image to a stream. Regardless of original format, the image is saved as png. Compressed image data is not supported. Return true on success.
    bool Save(Stream& dest) const override;

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
    /// Return the image format.
    ImageFormat Format() const { return format; }
    /// Return whether is a compressed image.
    bool IsCompressed() const { return format >= FMT_DXT1; }
    /// Return number of mip levels. Always 1 for uncompressed images.
    size_t NumLevels() const { return numLevels; }
    /// Calculate the next mip image with halved width and height. Supports uncompressed images only. Return true on success.
    bool GenerateMipImage(Image& dest) const;
    /// Return the data for a mip level. Uncompressed images can only return the first (index 0) level.
    ImageLevel Level(size_t levelIndex) const;
    /// Decompress a mip level as 8-bit RGBA. Supports compressed images only. Return true on success.
    bool DecompressLevel(unsigned char* dest, size_t levelIndex) const;

    /// Calculate the data size of an image level.
    static size_t CalculateDataSize(int width, int height, ImageFormat format, size_t* numRows = 0, size_t* rowSize = 0);

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
    /// Number of mip levels. 1 for uncompressed images.
    size_t numLevels;
    /// Image pixel data.
    AutoArrayPtr<unsigned char> data;
};

}
