// For conditions of distribution and use, see copyright notice in License.txt

#include "LightEnvironment.h"

LightEnvironment::LightEnvironment() :
    ambientColor(DEFAULT_AMBIENT_COLOR),
    fogColor(DEFAULT_FOG_COLOR),
    fogStart(DEFAULT_FOG_START),
    fogEnd(DEFAULT_FOG_END)
{
}

LightEnvironment::~LightEnvironment()
{
}

void LightEnvironment::RegisterObject()
{
    // Register allocator with small initial capacity with the assumption that we don't create many of them (unlike other scene nodes)
    RegisterFactory<LightEnvironment>(1);
    CopyBaseAttributes<LightEnvironment, Node>();
    RegisterDerivedType<LightEnvironment, Node>();
    RegisterRefAttribute<LightEnvironment>("ambientColor", &LightEnvironment::AmbientColor, &LightEnvironment::SetAmbientColor, DEFAULT_AMBIENT_COLOR);
    RegisterRefAttribute<LightEnvironment>("fogColor", &LightEnvironment::FogColor, &LightEnvironment::SetFogColor, DEFAULT_AMBIENT_COLOR);
    RegisterAttribute<LightEnvironment>("fogStart", &LightEnvironment::FogStart, &LightEnvironment::SetFogStart, DEFAULT_FOG_START);
    RegisterAttribute<LightEnvironment>("fogEnd", &LightEnvironment::FogEnd, &LightEnvironment::SetFogEnd, DEFAULT_FOG_END);
}

void LightEnvironment::SetAmbientColor(const Color& color)
{
    ambientColor = color;
}

void LightEnvironment::SetFogColor(const Color& color)
{
    fogColor = color;
}

void LightEnvironment::SetFogStart(float distance)
{
    fogStart = distance;
}

void LightEnvironment::SetFogEnd(float distance)
{
    fogEnd = distance;
}
