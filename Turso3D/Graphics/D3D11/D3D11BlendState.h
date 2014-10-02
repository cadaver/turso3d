// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of how to blend geometry into the framebuffer.
class BlendState : public GPUObject
{
public:
    /// Construct.
    BlendState();
    /// Destruct.
    virtual ~BlendState();

    /// Release the blend state object.
    virtual void Release();

    /// Define parameters and create the blend state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(bool blendEnable, BlendFactor srcBlend, BlendFactor destBlend, BlendOperation blendOp, BlendFactor srcBlendAlpha, BlendFactor destBlendAlpha, BlendOperation blendOpAlpha, unsigned char colorWriteMask = COLORMASK_ALL, bool alphaToCoverage = false);

    /// Return the D3D11 state object.
    void* StateObject() const { return stateObject; }
    /// Return blend enable flag.
    bool BlendEnable() const { return blendEnable; }
    /// Return source color blend factor.
    BlendFactor SrcBlend() const { return srcBlend; }
    /// Return destination color blend factor.
    BlendFactor DestBlend() const { return destBlend; }
    /// Return color blend operation.
    BlendOperation BlendOp() const { return blendOp; }
    /// Return source alpha blend factor.
    BlendFactor SrcBlendAlpha() const { return srcBlendAlpha; }
    /// Return destination alpha blend factor.
    BlendFactor DestBlendAlpha() const { return destBlendAlpha; }
    /// Return alpha blend operation.
    BlendOperation BlendOpAlpha() const { return blendOpAlpha; }
    /// Return color write mask.
    unsigned char ColorWriteMask() const { return colorWriteMask; }
    /// Return alpha to coverage flag.
    bool AlphaToCoverage() const { return alphaToCoverage; }

private:
    /// D3D11 blend state object.
    void* stateObject;
    /// Source color blend factor.
    BlendFactor srcBlend;
    /// Destination color blend factor.
    BlendFactor destBlend;
    /// Color blend operation.
    BlendOperation blendOp;
    /// Source alpha blend factor.
    BlendFactor srcBlendAlpha;
    /// Destination alpha blend factor.
    BlendFactor destBlendAlpha;
    /// Color blend operation.
    BlendOperation blendOpAlpha;
    /// Rendertarget color write mask.
    unsigned char colorWriteMask;
    /// Blend enable flag.
    bool blendEnable;
    /// Alpha to coverage flag.
    bool alphaToCoverage;
};

}