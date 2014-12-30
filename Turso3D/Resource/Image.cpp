// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/Stream.h"
#include "../Math/Math.h"
#include "Decompress.h"

#include <cstdlib>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

#include "../Debug/DebugNew.h"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((unsigned)(ch0) | ((unsigned)(ch1) << 8) | ((unsigned)(ch2) << 16) | ((unsigned)(ch3) << 24))
#endif

#define FOURCC_DXT1 (MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2 (MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3 (MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4 (MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5 (MAKEFOURCC('D','X','T','5'))

namespace Turso3D
{

const int Image::components[] =
{
    0,      // FMT_NONE
    1,      // FMT_R8
    2,      // FMT_RG8
    4,      // FMT_RGBA8
    1,      // FMT_A8
    0,      // FMT_R16
    0,      // FMT_RG16
    0,      // FMT_RGBA16
    0,      // FMT_R16F
    0,      // FMT_RG16F
    0,      // FMT_RGBA16F
    0,      // FMT_R32F
    0,      // FMT_RG32F
    0,      // FMT_RGB32F
    0,      // FMT_RGBA32F
    0,      // FMT_D16
    0,      // FMT_D32
    0,      // FMT_D24S8
    0,      // FMT_DXT1
    0,      // FMT_DXT3
    0,      // FMT_DXT5
    0,      // FMT_ETC1
    0,      // FMT_PVRTC_RGB_2BPP
    0,      // FMT_PVRTC_RGBA_2BPP
    0,      // FMT_PVRTC_RGB_4BPP
    0       // FMT_PVRTC_RGBA_4BPP
};

const size_t Image::pixelByteSizes[] =
{
    0,      // FMT_NONE
    1,      // FMT_R8
    2,      // FMT_RG8
    4,      // FMT_RGBA8
    1,      // FMT_A8
    2,      // FMT_R16
    4,      // FMT_RG16
    8,      // FMT_RGBA16
    2,      // FMT_R16F
    4,      // FMT_RG16F
    8,      // FMT_RGBA16F
    4,      // FMT_R32F
    8,      // FMT_RG32F
    12,     // FMT_RGB32F
    16,     // FMT_RGBA32F
    2,      // FMT_D16
    4,      // FMT_D32
    4,      // FMT_D24S8
    0,      // FMT_DXT1
    0,      // FMT_DXT3
    0,      // FMT_DXT5
    0,      // FMT_ETC1
    0,      // FMT_PVRTC_RGB_2BPP
    0,      // FMT_PVRTC_RGBA_2BPP
    0,      // FMT_PVRTC_RGB_4BPP
    0       // FMT_PVRTC_RGBA_4BPP
};

static const ImageFormat componentsToFormat[] =
{
    FMT_NONE,
    FMT_R8,
    FMT_RG8,
    FMT_RGBA8,
    FMT_RGBA8
};

/// \cond PRIVATE
struct DDColorKey
{
    unsigned dwColorSpaceLowValue;
    unsigned dwColorSpaceHighValue;
};
/// \endcond

/// \cond PRIVATE
struct DDPixelFormat
{
    unsigned dwSize;
    unsigned dwFlags;
    unsigned dwFourCC;
    union
    {
        unsigned dwRGBBitCount;
        unsigned dwYUVBitCount;
        unsigned dwZBufferBitDepth;
        unsigned dwAlphaBitDepth;
        unsigned dwLuminanceBitCount;
        unsigned dwBumpBitCount;
        unsigned dwPrivateFormatBitCount;
    };
    union
    {
        unsigned dwRBitMask;
        unsigned dwYBitMask;
        unsigned dwStencilBitDepth;
        unsigned dwLuminanceBitMask;
        unsigned dwBumpDuBitMask;
        unsigned dwOperations;
    };
    union
    {
        unsigned dwGBitMask;
        unsigned dwUBitMask;
        unsigned dwZBitMask;
        unsigned dwBumpDvBitMask;
        struct
        {
            unsigned short wFlipMSTypes;
            unsigned short wBltMSTypes;
        } multiSampleCaps;
    };
    union
    {
        unsigned dwBBitMask;
        unsigned dwVBitMask;
        unsigned dwStencilBitMask;
        unsigned dwBumpLuminanceBitMask;
    };
    union
    {
        unsigned dwRGBAlphaBitMask;
        unsigned dwYUVAlphaBitMask;
        unsigned dwLuminanceAlphaBitMask;
        unsigned dwRGBZBitMask;
        unsigned dwYUVZBitMask;
    };
};
/// \endcond

/// \cond PRIVATE
struct DDSCaps2
{
    unsigned dwCaps;
    unsigned dwCaps2;
    unsigned dwCaps3;
    union
    {
        unsigned dwCaps4;
        unsigned dwVolumeDepth;
    };
};
/// \endcond

/// \cond PRIVATE
struct DDSurfaceDesc2
{
    unsigned dwSize;
    unsigned dwFlags;
    unsigned dwHeight;
    unsigned dwWidth;
    union
    {
        unsigned lPitch;
        unsigned dwLinearSize;
    };
    union
    {
        unsigned dwBackBufferCount;
        unsigned dwDepth;
    };
    union
    {
        unsigned dwMipMapCount;
        unsigned dwRefreshRate;
        unsigned dwSrcVBHandle;
    };
    unsigned dwAlphaBitDepth;
    unsigned dwReserved;
    unsigned lpSurface; // Do not define as a void pointer, as it is 8 bytes in a 64bit build
    union
    {
        DDColorKey ddckCKDestOverlay;
        unsigned dwEmptyFaceColor;
    };
    DDColorKey ddckCKDestBlt;
    DDColorKey ddckCKSrcOverlay;
    DDColorKey ddckCKSrcBlt;
    union
    {
        DDPixelFormat ddpfPixelFormat;
        unsigned dwFVF;
    };
    DDSCaps2 ddsCaps;
    unsigned dwTextureStage;
};
/// \endcond

Image::Image() :
    width(0),
    height(0),
    format(FMT_NONE),
    numLevels(1)
{
}

Image::~Image()
{
}

void Image::RegisterObject()
{
    RegisterFactory<Image>();
}

bool Image::BeginLoad(Stream& source)
{
    PROFILE(LoadImage);

    // Check for DDS, KTX or PVR compressed format
    String fileID = source.ReadFileID();

    if (fileID == "DDS ")
    {
        // DDS compressed format
        DDSurfaceDesc2 ddsd;
        source.Read(&ddsd, sizeof(ddsd));

        switch (ddsd.ddpfPixelFormat.dwFourCC)
        {
        case FOURCC_DXT1:
            format = FMT_DXT1;
            break;

        case FOURCC_DXT3:
            format = FMT_DXT3;
            break;

        case FOURCC_DXT5:
            format = FMT_DXT5;
            break;

        default:
            LOGERROR("Unsupported DDS format");
            return false;
        }

        size_t dataSize = source.Size() - source.Position();
        data = new unsigned char[dataSize];
        width = ddsd.dwWidth;
        height = ddsd.dwHeight;
        numLevels = ddsd.dwMipMapCount ? ddsd.dwMipMapCount : 1;
        source.Read(data.Get(), dataSize);
    }
    else if (fileID == "\253KTX")
    {
        source.Seek(12);

        unsigned endianness = source.Read<unsigned>();
        unsigned type = source.Read<unsigned>();
        /* unsigned typeSize = */ source.Read<unsigned>();
        unsigned imageFormat = source.Read<unsigned>();
        unsigned internalFormat = source.Read<unsigned>();
        /* unsigned baseInternalFormat = */ source.Read<unsigned>();
        unsigned imageWidth = source.Read<unsigned>();
        unsigned imageHeight = source.Read<unsigned>();
        unsigned depth = source.Read<unsigned>();
        /* unsigned arrayElements = */ source.Read<unsigned>();
        unsigned faces = source.Read<unsigned>();
        unsigned mipmaps = source.Read<unsigned>();
        unsigned keyValueBytes = source.Read<unsigned>();

        if (endianness != 0x04030201)
        {
            LOGERROR("Big-endian KTX files not supported");
            return false;
        }

        if (type != 0 || imageFormat != 0)
        {
            LOGERROR("Uncompressed KTX files not supported");
            return false;
        }

        if (faces > 1 || depth > 1)
        {
            LOGERROR("3D or cube KTX files not supported");
            return false;
        }

        if (mipmaps == 0)
        {
            LOGERROR("KTX files without explicitly specified mipmap count not supported");
            return false;
        }

        format = FMT_NONE;
        switch (internalFormat)
        {
        case 0x83f1:
            format = FMT_DXT1;
            break;

        case 0x83f2:
            format = FMT_DXT3;
            break;

        case 0x83f3:
            format = FMT_DXT5;
            break;

        case 0x8d64:
            format = FMT_ETC1;
            break;

        case 0x8c00:
            format = FMT_PVRTC_RGB_4BPP;
            break;

        case 0x8c01:
            format = FMT_PVRTC_RGB_2BPP;
            break;

        case 0x8c02:
            format = FMT_PVRTC_RGBA_4BPP;
            break;

        case 0x8c03:
            format = FMT_PVRTC_RGBA_2BPP;
            break;
        }

        if (format == FMT_NONE)
        {
            LOGERROR("Unsupported texture format in KTX file");
            return false;
        }

        source.Seek(source.Position() + keyValueBytes);
        size_t dataSize = source.Size() - source.Position() - mipmaps * sizeof(unsigned);

        data = new unsigned char[dataSize];
        width = imageWidth;
        height = imageHeight;
        numLevels = mipmaps;

        size_t dataOffset = 0;
        for (size_t i = 0; i < mipmaps; ++i)
        {
            size_t levelSize = source.Read<unsigned>();
            if (levelSize + dataOffset > dataSize)
            {
                LOGERROR("KTX mipmap level data size exceeds file size");
                return false;
            }

            source.Read(&data[dataOffset], levelSize);
            dataOffset += levelSize;
            if (source.Position() & 3)
                source.Seek((source.Position() + 3) & 0xfffffffc);
        }
    }
    else if (fileID == "PVR\3")
    {
        /* unsigned flags = */ source.Read<unsigned>();
        unsigned pixelFormatLo = source.Read<unsigned>();
        /* unsigned pixelFormatHi = */ source.Read<unsigned>();
        /* unsigned colourSpace = */ source.Read<unsigned>();
        /* unsigned channelType = */ source.Read<unsigned>();
        unsigned imageHeight = source.Read<unsigned>();
        unsigned imageWidth = source.Read<unsigned>();
        unsigned depth = source.Read<unsigned>();
        /* unsigned numSurfaces = */ source.Read<unsigned>();
        unsigned numFaces = source.Read<unsigned>();
        unsigned mipmapCount = source.Read<unsigned>();
        unsigned metaDataSize = source.Read<unsigned>();

        if (depth > 1 || numFaces > 1)
        {
            LOGERROR("3D or cube PVR files not supported");
            return false;
        }

        if (mipmapCount == 0)
        {
            LOGERROR("PVR files without explicitly specified mipmap count not supported");
            return false;
        }

        format = FMT_NONE;
        switch (pixelFormatLo)
        {
        case 0:
            format = FMT_PVRTC_RGB_2BPP;
            break;

        case 1:
            format = FMT_PVRTC_RGBA_2BPP;
            break;

        case 2:
            format = FMT_PVRTC_RGB_4BPP;
            break;

        case 3:
            format = FMT_PVRTC_RGBA_4BPP;
            break;

        case 6:
            format = FMT_ETC1;
            break;

        case 7:
            format = FMT_DXT1;
            break;

        case 9:
            format = FMT_DXT3;
            break;

        case 11:
            format = FMT_DXT5;
            break;
        }

        if (format == FMT_NONE)
        {
            LOGERROR("Unsupported texture format in PVR file");
            return false;
        }

        source.Seek(source.Position() + metaDataSize);
        size_t dataSize = source.Size() - source.Position();

        data = new unsigned char[dataSize];
        width = imageWidth;
        height = imageHeight;
        numLevels = mipmapCount;

        source.Read(data.Get(), dataSize);
    }
    else
    {
        // Not DDS, KTX or PVR, use STBImage to load other image formats as uncompressed
        source.Seek(0);
        int imageWidth, imageHeight;
        unsigned imageComponents;
        unsigned char* pixelData = DecodePixelData(source, imageWidth, imageHeight, imageComponents);
        if (!pixelData)
        {
            LOGERROR("Could not load image " + source.Name() + ": " + String(stbi_failure_reason()));
            return false;
        }
        
        SetSize(imageWidth, imageHeight, componentsToFormat[imageComponents]);

        if (imageComponents != 3)
            SetData(pixelData);
        else
        {
            // Convert RGB to RGBA as for example Direct3D 11 does not support 24-bit formats
            AutoArrayPtr<unsigned char> rgbaData(new unsigned char[4 * imageWidth * imageHeight]);
            unsigned char* src = pixelData;
            unsigned char* dest = rgbaData.Get();
            for (int i = 0; i < imageWidth * imageHeight; ++i)
            {
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = 0xff;
            }

            SetData(rgbaData.Get());
        }

        FreePixelData(pixelData);
    }

    return true;
}

bool Image::Save(Stream& dest)
{
    PROFILE(SaveImage);

    if (IsCompressed())
    {
        LOGERROR("Can not save compressed image " + Name());
        return false;
    }

    if (!data)
    {
        LOGERROR("Can not save zero-sized image " + Name());
        return false;
    }

    int components = (int)PixelByteSize();
    if (components < 1 || components > 4)
    {
        LOGERROR("Unsupported pixel format for PNG save on image " + Name());
        return false;
    }

    int len;
    unsigned char *png = stbi_write_png_to_mem(data.Get(), 0, width, height, components, &len);
    bool success = dest.Write(png, len) == (size_t)len;
    free(png);
    return success;
}

void Image::SetSize(int newWidth, int newHeight, ImageFormat newFormat)
{
    if (newWidth == width && newHeight == height && newFormat == format)
        return;

    if (newWidth <= 0 || newHeight <= 0)
    {
        LOGERROR("Can not set negative image size");
        return;
    }
    if (pixelByteSizes[newFormat] == 0)
    {
        LOGERROR("Can not set image size with unspecified pixel byte size (including compressed formats)");
        return;
    }

    data = new unsigned char[newWidth * newHeight * pixelByteSizes[newFormat]];
    width = newWidth;
    height = newHeight;
    format = newFormat;
    numLevels = 1;
}

void Image::SetData(const unsigned char* pixelData)
{
    if (!IsCompressed())
        memcpy(data.Get(), pixelData, width * height * PixelByteSize());
    else
        LOGERROR("Can not set pixel data of a compressed image");
}

unsigned char* Image::DecodePixelData(Stream& source, int& width, int& height, unsigned& components)
{
    size_t dataSize = source.Size();

    AutoArrayPtr<unsigned char> buffer(new unsigned char[dataSize]);
    source.Read(buffer.Get(), dataSize);
    return stbi_load_from_memory(buffer.Get(), (int)dataSize, &width, &height, (int *)&components, 0);
}

void Image::FreePixelData(unsigned char* pixelData)
{
    if (!pixelData)
        return;

    stbi_image_free(pixelData);
}

bool Image::GenerateMipImage(Image& dest) const
{
    PROFILE(GenerateMipImage);

    int components = Components();
    if (components < 1 || components > 4)
    {
        LOGERROR("Unsupported format for calculating the next mip level");
        return false;
    }

    int widthOut = width / 2;
    int heightOut = height / 2;

    if (widthOut < 1)
        widthOut = 1;
    if (heightOut < 1)
        heightOut = 1;

    dest.SetSize(widthOut, heightOut, format);

    const unsigned char* pixelDataIn = data.Get();
    unsigned char* pixelDataOut = dest.data.Get();

    // 1D case
    if (height == 1 || width == 1)
    {
        // Loop using the larger dimension
        if (widthOut < heightOut)
            widthOut = heightOut;

        switch (components)
        {
        case 1:
            for (int x = 0; x < widthOut; ++x)
                pixelDataOut[x] = ((unsigned)pixelDataIn[x*2] + pixelDataIn[x*2+1]) >> 1;
            break;

        case 2:
            for (int x = 0; x < widthOut*2; x += 2)
            {
                pixelDataOut[x] = ((unsigned)pixelDataIn[x*2] + pixelDataIn[x*2+2]) >> 1;
                pixelDataOut[x+1] = ((unsigned)pixelDataIn[x*2+1] + pixelDataIn[x*2+3]) >> 1;
            }
            break;

        case 4:
            for (int x = 0; x < widthOut*4; x += 4)
            {
                pixelDataOut[x] = ((unsigned)pixelDataIn[x*2] + pixelDataIn[x*2+4]) >> 1;
                pixelDataOut[x+1] = ((unsigned)pixelDataIn[x*2+1] + pixelDataIn[x*2+5]) >> 1;
                pixelDataOut[x+2] = ((unsigned)pixelDataIn[x*2+2] + pixelDataIn[x*2+6]) >> 1;
                pixelDataOut[x+3] = ((unsigned)pixelDataIn[x*2+3] + pixelDataIn[x*2+7]) >> 1;
            }
            break;
        }
    }
    // 2D case
    else
    {
        switch (components)
        {
        case 1:
            for (int y = 0; y < heightOut; ++y)
            {
                const unsigned char* inUpper = &pixelDataIn[(y*2)*width];
                const unsigned char* inLower = &pixelDataIn[(y*2+1)*width];
                unsigned char* out = &pixelDataOut[y*widthOut];

                for (int x = 0; x < widthOut; ++x)
                    out[x] = ((unsigned)inUpper[x*2] + inUpper[x*2+1] + inLower[x*2] + inLower[x*2+1]) >> 2;
            }
            break;

        case 2:
            for (int y = 0; y < heightOut; ++y)
            {
                const unsigned char* inUpper = &pixelDataIn[(y*2)*width*2];
                const unsigned char* inLower = &pixelDataIn[(y*2+1)*width*2];
                unsigned char* out = &pixelDataOut[y*widthOut*2];

                for (int x = 0; x < widthOut*2; x += 2)
                {
                    out[x] = ((unsigned)inUpper[x*2] + inUpper[x*2+2] + inLower[x*2] + inLower[x*2+2]) >> 2;
                    out[x+1] = ((unsigned)inUpper[x*2+1] + inUpper[x*2+3] + inLower[x*2+1] + inLower[x*2+3]) >> 2;
                }
            }
            break;

        case 4:
            for (int y = 0; y < heightOut; ++y)
            {
                const unsigned char* inUpper = &pixelDataIn[(y*2)*width*4];
                const unsigned char* inLower = &pixelDataIn[(y*2+1)*width*4];
                unsigned char* out = &pixelDataOut[y*widthOut*4];

                for (int x = 0; x < widthOut*4; x += 4)
                {
                    out[x] = ((unsigned)inUpper[x*2] + inUpper[x*2+4] + inLower[x*2] + inLower[x*2+4]) >> 2;
                    out[x+1] = ((unsigned)inUpper[x*2+1] + inUpper[x*2+5] + inLower[x*2+1] + inLower[x*2+5]) >> 2;
                    out[x+2] = ((unsigned)inUpper[x*2+2] + inUpper[x*2+6] + inLower[x*2+2] + inLower[x*2+6]) >> 2;
                    out[x+3] = ((unsigned)inUpper[x*2+3] + inUpper[x*2+7] + inLower[x*2+3] + inLower[x*2+7]) >> 2;
                }
            }
            break;
        }
    }

    return true;
}

ImageLevel Image::Level(size_t index) const
{
    ImageLevel level;

    if (index >= numLevels)
        return level;

    size_t i = 0;
    size_t offset = 0;

    for (;;)
    {
        level.width = Max(width >> i, 1);
        level.height = Max(height >> i, 1);
        level.data = data.Get() + offset;

        size_t dataSize = CalculateDataSize(level.width, level.height, format, &level.rows, &level.rowSize);
        if (i == index)
            return level;

        offset += dataSize;
        ++i;
    }
}

bool Image::DecompressLevel(unsigned char* dest, size_t index) const
{
    PROFILE(DecompressImageLevel);

    if (!dest)
    {
        LOGERROR("Null destination data for DecompressLevel");
        return false;
    }

    if (index >= numLevels)
    {
        LOGERROR("Mip level index out of bounds for DecompressLevel");
        return false;
    }

    ImageLevel level = Level(index);

    switch (format)
    {
    case FMT_DXT1:
    case FMT_DXT3:
    case FMT_DXT5:
        DecompressImageDXT(dest, level.data, level.width, level.height, format);
        break;

    case FMT_ETC1:
        DecompressImageETC(dest, level.data, level.width, level.height);
        break;

    case FMT_PVRTC_RGB_2BPP:
    case FMT_PVRTC_RGBA_2BPP:
    case FMT_PVRTC_RGB_4BPP:
    case FMT_PVRTC_RGBA_4BPP:
        DecompressImagePVRTC(dest, level.data, level.width, level.height, format);
        break;

    default:
        LOGERROR("Unsupported format for DecompressLevel");
        return false;
    }

    return true;
}

size_t Image::CalculateDataSize(int width, int height, ImageFormat format, size_t* dstRows, size_t* dstRowSize)
{
    size_t rows, rowSize, dataSize;

    if (format < FMT_DXT1)
    {
        rows = height;
        rowSize = width * pixelByteSizes[format];
        dataSize = rows * rowSize;
    }
    else if (format < FMT_PVRTC_RGB_2BPP)
    {
        size_t blockSize = (format == FMT_DXT1 || format == FMT_ETC1) ? 8 : 16;
        rows = (height + 3) / 4;
        rowSize = ((width + 3) / 4) * blockSize;
        dataSize = rows * rowSize;
    }
    else
    {
        size_t blockSize = format < FMT_PVRTC_RGB_4BPP ? 2 : 4;
        size_t dataWidth = Max(width, blockSize == 2 ? 16 : 8);
        rows = Max(height, 8);
        dataSize = (dataWidth * rows * blockSize + 7) >> 3;
        rowSize = dataSize / rows;
    }

    if (dstRows)
        *dstRows = rows;
    if (dstRowSize)
        *dstRowSize = rowSize;
    return dataSize;
}

}
