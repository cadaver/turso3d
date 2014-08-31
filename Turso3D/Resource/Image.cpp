// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/File.h"
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

/// DirectDraw color key definition.
struct DDColorKey
{
    unsigned dwColorSpaceLowValue;
    unsigned dwColorSpaceHighValue;
};

/// DirectDraw pixel format definition.
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

/// DirectDraw surface capabilities.
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

/// DirectDraw surface description.
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

bool CompressedLevel::Decompress(unsigned char* dest)
{
    PROFILE(DecompressImage);

    if (!data)
        return false;

    switch (format)
    {
    case CF_DXT1:
    case CF_DXT3:
    case CF_DXT5:
        DecompressImageDXT(dest, data, width, height, format);
        return true;

    case CF_ETC1:
        DecompressImageETC(dest, data, width, height);
        return true;

    case CF_PVRTC_RGB_2BPP:
    case CF_PVRTC_RGBA_2BPP:
    case CF_PVRTC_RGB_4BPP:
    case CF_PVRTC_RGBA_4BPP:
        DecompressImagePVRTC(dest, data, width, height, format);
        return true;

    default:
         // Unknown format
         return false;
    }
}

Image::Image() :
    width(0),
    height(0),
    components(0)
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
            compressedFormat = CF_DXT1;
            components = 3;
            break;

        case FOURCC_DXT3:
            compressedFormat = CF_DXT3;
            components = 4;
            break;

        case FOURCC_DXT5:
            compressedFormat = CF_DXT5;
            components = 4;
            break;

        default:
            LOGERROR("Unsupported DDS format");
            return false;
        }

        size_t dataSize = source.Size() - source.Position();
        data = new unsigned char[dataSize];
        width = ddsd.dwWidth;
        height = ddsd.dwHeight;
        numCompressedLevels = ddsd.dwMipMapCount;
        if (!numCompressedLevels)
            numCompressedLevels = 1;
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

        compressedFormat = CF_NONE;
        switch (internalFormat)
        {
        case 0x83f1:
            compressedFormat = CF_DXT1;
            components = 4;
            break;

        case 0x83f2:
            compressedFormat = CF_DXT3;
            components = 4;
            break;

        case 0x83f3:
            compressedFormat = CF_DXT5;
            components = 4;
            break;

        case 0x8d64:
            compressedFormat = CF_ETC1;
            components = 3;
            break;

        case 0x8c00:
            compressedFormat = CF_PVRTC_RGB_4BPP;
            components = 3;
            break;

        case 0x8c01:
            compressedFormat = CF_PVRTC_RGB_2BPP;
            components = 3;
            break;

        case 0x8c02:
            compressedFormat = CF_PVRTC_RGBA_4BPP;
            components = 4;
            break;

        case 0x8c03:
            compressedFormat = CF_PVRTC_RGBA_2BPP;
            components = 4;
            break;
        }

        if (compressedFormat == CF_NONE)
        {
            LOGERROR("Unsupported texture format in KTX file");
            return false;
        }

        source.Seek(source.Position() + keyValueBytes);
        size_t dataSize = source.Size() - source.Position() - mipmaps * sizeof(unsigned);

        data = new unsigned char[dataSize];
        width = imageWidth;
        height = imageHeight;
        numCompressedLevels = mipmaps;

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

        compressedFormat = CF_NONE;
        switch (pixelFormatLo)
        {
        case 0:
            compressedFormat = CF_PVRTC_RGB_2BPP;
            components = 3;
            break;

        case 1:
            compressedFormat = CF_PVRTC_RGBA_2BPP;
            components = 4;
            break;

        case 2:
            compressedFormat = CF_PVRTC_RGB_4BPP;
            components = 3;
            break;

        case 3:
            compressedFormat = CF_PVRTC_RGBA_4BPP;
            components = 4;
            break;

        case 6:
            compressedFormat = CF_ETC1;
            components = 3;
            break;

        case 7:
            compressedFormat = CF_DXT1;
            components = 4;
            break;

        case 9:
            compressedFormat = CF_DXT3;
            components = 4;
            break;

        case 11:
            compressedFormat = CF_DXT5;
            components = 4;
            break;
        }

        if (compressedFormat == CF_NONE)
        {
            LOGERROR("Unsupported texture format in PVR file");
            return false;
        }

        source.Seek(source.Position() + metaDataSize);
        size_t dataSize = source.Size() - source.Position();

        data = new unsigned char[dataSize];
        width = imageWidth;
        height = imageHeight;
        numCompressedLevels = mipmapCount;

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
        SetSize(imageWidth, imageHeight, imageComponents);
        SetData(pixelData);
        FreePixelData(pixelData);
    }

    return true;
}

bool Image::Save(Stream& dest) const
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

    int len;
    unsigned char *png = stbi_write_png_to_mem(data.Get(), 0, width, height, components, &len);
    bool success = dest.Write(png, len) == (size_t)len;
    free(png);
    return success;
}

void Image::SetSize(int newWidth, int newHeight, unsigned newComponents)
{
    if (newWidth == width && newHeight == height && newComponents == components)
        return;

    if (newWidth <= 0 || newHeight <= 0)
        return;

    data = new unsigned char[newWidth * newHeight * newComponents];
    width = newWidth;
    height = newHeight;
    components = newComponents;
    compressedFormat = CF_NONE;
    numCompressedLevels = 0;
}

void Image::SetData(const unsigned char* pixelData)
{
    if (!IsCompressed())
        memcpy(data.Get(), pixelData, width * height * components);
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

AutoPtr<Image> Image::GenerateNextMipLevel() const
{
    PROFILE(GenerateMipLevel);

    if (IsCompressed())
    {
        LOGERROR("Can not generate mip level from compressed data");
        return AutoPtr<Image>();
    }
    if (components < 1 || components > 4)
    {
        LOGERROR("Illegal number of image components for mip level generation");
        return AutoPtr<Image>();
    }

    int widthOut = width / 2;
    int heightOut = height / 2;

    if (widthOut < 1)
        widthOut = 1;
    if (heightOut < 1)
        heightOut = 1;

    AutoPtr<Image> ret(new Image());
    ret->SetSize(widthOut, heightOut, components);

    const unsigned char* pixelDataIn = data.Get();
    unsigned char* pixelDataOut = ret->data.Get();

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

        case 3:
            for (int x = 0; x < widthOut*3; x += 3)
            {
                pixelDataOut[x] = ((unsigned)pixelDataIn[x*2] + pixelDataIn[x*2+3]) >> 1;
                pixelDataOut[x+1] = ((unsigned)pixelDataIn[x*2+1] + pixelDataIn[x*2+4]) >> 1;
                pixelDataOut[x+2] = ((unsigned)pixelDataIn[x*2+2] + pixelDataIn[x*2+5]) >> 1;
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
                {
                    out[x] = ((unsigned)inUpper[x*2] + inUpper[x*2+1] + inLower[x*2] + inLower[x*2+1]) >> 2;
                }
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

        case 3:
            for (int y = 0; y < heightOut; ++y)
            {
                const unsigned char* inUpper = &pixelDataIn[(y*2)*width*3];
                const unsigned char* inLower = &pixelDataIn[(y*2+1)*width*3];
                unsigned char* out = &pixelDataOut[y*widthOut*3];

                for (int x = 0; x < widthOut*3; x += 3)
                {
                    out[x] = ((unsigned)inUpper[x*2] + inUpper[x*2+3] + inLower[x*2] + inLower[x*2+3]) >> 2;
                    out[x+1] = ((unsigned)inUpper[x*2+1] + inUpper[x*2+4] + inLower[x*2+1] + inLower[x*2+4]) >> 2;
                    out[x+2] = ((unsigned)inUpper[x*2+2] + inUpper[x*2+5] + inLower[x*2+2] + inLower[x*2+5]) >> 2;
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

    return ret;
}

CompressedLevel Image::CompressedMipLevel(size_t index) const
{
    CompressedLevel level;

    if (compressedFormat == CF_NONE)
    {
        LOGERROR("Image is not compressed");
        return level;
    }
    if (index >= numCompressedLevels)
    {
        LOGERROR("Compressed image mip level out of bounds");
        return level;
    }

    level.format = compressedFormat;
    level.width = width;
    level.height = height;

    if (compressedFormat < CF_PVRTC_RGB_2BPP)
    {
        level.blockSize = (compressedFormat == CF_DXT1 || compressedFormat == CF_ETC1) ? 8 : 16;
        size_t i = 0;
        size_t offset = 0;

        for (;;)
        {
            if (!level.width)
                level.width = 1;
            if (!level.height)
                level.height = 1;

            level.rowSize = ((level.width + 3) / 4) * level.blockSize;
            level.rows = ((level.height + 3) / 4);
            level.data = data.Get() + offset;
            level.dataSize = level.rows * level.rowSize;

            if (i == index)
                return level;

            offset += level.dataSize;
            level.width /= 2;
            level.height /= 2;
            ++i;
        }
    }
    else
    {
        level.blockSize = compressedFormat < CF_PVRTC_RGB_4BPP ? 2 : 4;
        size_t i = 0;
        size_t offset = 0;

        for (;;)
        {
            if (!level.width)
                level.width = 1;
            if (!level.height)
                level.height = 1;

            int dataWidth = Max(level.width, level.blockSize == 2 ? 16 : 8);
            int dataHeight = Max(level.height, 8);
            level.data = data.Get() + offset;
            level.dataSize = (dataWidth * dataHeight * level.blockSize + 7) >> 3;
            level.rows = dataHeight;
            level.rowSize = level.dataSize / level.rows;

            if (i == index)
                return level;

            offset += level.dataSize;
            level.width /= 2;
            level.height /= 2;
            ++i;
        }
    }
}

}
