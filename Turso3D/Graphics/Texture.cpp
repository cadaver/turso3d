// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "Texture.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

void Texture::RegisterObject()
{
    RegisterFactory<Texture>();
}

bool Texture::BeginLoad(Stream& source)
{
    loadImages.Clear();
    loadImages.Push(new Image());
    if (!loadImages[0]->Load(source))
    {
        loadImages.Clear();
        return false;
    }

    // If image uses unsupported format, decompress to RGBA now
    if (loadImages[0]->Format() >= FMT_ETC1)
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
            loadImages.Push(new Image());
            mipImage->GenerateMipImage(*loadImages.Back());
            mipImage = loadImages.Back();
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
    bool success = Define(TEX_2D, USAGE_IMMUTABLE, image->Size(), image->Format(), initialData.Size(), &initialData[0]);
    /// \todo Read a parameter file for the sampling parameters
    success &= DefineSampler(FILTER_TRILINEAR, ADDRESS_WRAP, ADDRESS_WRAP, ADDRESS_WRAP, 16, 0.0f, M_INFINITY, Color::BLACK);

    loadImages.Clear();
    return success;
}

}
