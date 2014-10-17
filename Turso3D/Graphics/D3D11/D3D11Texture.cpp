// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "D3D11Graphics.h"
#include "D3D11Texture.h"

namespace Turso3D
{

Texture::Texture() :
    texture(0),
    resourceView(0),
    sampler(0)
{
}

Texture::~Texture()
{
    Release();
}

bool Texture::BeginLoad(Stream& source)
{
    return true;
}

bool Texture::EndLoad()
{
    return true;
}

void Texture::Release()
{
}

}
