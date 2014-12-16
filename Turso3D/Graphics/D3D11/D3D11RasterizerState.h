// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of how to rasterize geometry into the framebuffer.
class TURSO3D_API RasterizerState : public RefCounted, public GPUObject
{
public:
    /// Construct.
    RasterizerState();
    /// Destruct.
    ~RasterizerState();

    /// Release the rasterizer state object.
    void Release() override;

    /// Define parameters and create the rasterizer state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(FillMode fillMode = FILL_SOLID, CullMode cullMode = CULL_BACK, int depthBias = 0, float depthBiasClamp = M_INFINITY, float slopeScaledDepthBias = 0.0f, bool depthClipEnable = true, bool scissorEnable = false);

    /// Return the D3D11 state object. Used internally and should not be called by portable application code.
    void* D3DState() const { return stateObject; }

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
    
private:
    /// D3D11 rasterizer state object.
    void* stateObject;
};

}