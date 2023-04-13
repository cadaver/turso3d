// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Scene/Node.h"

static const Color DEFAULT_AMBIENT_COLOR(0.1f, 0.1f, 0.1f);
static const Color DEFAULT_FOG_COLOR(Color::BLACK);
static const float DEFAULT_FOG_START = 500.0f;
static const float DEFAULT_FOG_END = 1000.0f;

/// Global lighting settings. Should be created as a child of the scene root.
class LightEnvironment : public Node
{
    OBJECT(LightEnvironment);

public:
    /// Construct.
    LightEnvironment();
    /// Destruct.
    ~LightEnvironment();

    /// Register factory and attributes.
    static void RegisterObject();

    /// Set ambient light color.
    void SetAmbientColor(const Color& color);
    /// Set fog end color.
    void SetFogColor(const Color& color);
    /// Set fog start distance.
    void SetFogStart(float distance);
    /// Set fog end distance.
    void SetFogEnd(float distance);

    /// Return ambient light color.
    const Color& AmbientColor() const { return ambientColor; }
    /// Return fog end color.
    const Color& FogColor() const { return fogColor; }
    /// Return fog start distance.
    float FogStart() const { return fogStart; }
    /// Return fog end distance.
    float FogEnd() const { return fogEnd; }

private:
    /// Ambient light color.
    Color ambientColor;
    /// Fog end color.
    Color fogColor;
    /// Fog start distance.
    float fogStart;
    /// Fog end distance.
    float fogEnd;
};
