// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of how to rasterize geometry into the framebuffer.
class RasterizerState : public GPUObject
{
public:
    /// Construct.
    RasterizerState();
    /// Destruct.
    virtual ~RasterizerState();

    /// Release the rasterizer state object.
    virtual void Release();

    /// Define parameters and create the rasterizer state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(FillMode fillMode = FILL_SOLID, CullMode cullMode = CULL_BACK, int depthBias = 0, float depthBiasClamp = 0.0f, float slopeScaledDepthBias = 0.0f, bool depthClipEnable = true, bool scissorEnable = false, bool multisampleEnable = false, bool antialiasedLineEnable = false);

    /// Return the D3D11 state object.
    void* StateObject() const { return stateObject; }

    /// Fill mode.
    FillMode fillMode;
    /// Culling mode.
    CullMode cullMode;
    /// Depth bias added to fragments.
    int depthBias;
    /// Maximum depth bias that can be added.
    float depthBiasClamp;
    /// Slope scaled depth bias.
    float slopeScaledDepthBias;
    /// Depth clipping flag.
    bool depthClipEnable;
    /// Scissor test flag.
    bool scissorEnable;
    /// Quadrilateral line anti-aliasing flag.
    bool multisampleEnable;
    /// Line antialiasing flag. Only effective if multisampleEnable is false.
    bool antialiasedLineEnable;

private:
    /// D3D11 rasterizer state object.
    void* stateObject;
};

}