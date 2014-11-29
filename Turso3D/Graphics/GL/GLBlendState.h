// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of how to blend geometry into the framebuffer.
class TURSO3D_API BlendState : public WeakRefCounted, public GPUObject
{
public:
    /// Construct.
    BlendState();
    /// Destruct.
    ~BlendState();

    /// Release the blend state object.
    void Release() override;

    /// Define parameters and create the blend state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(bool blendEnable = false, BlendFactor srcBlend = BLEND_ONE, BlendFactor destBlend = BLEND_ONE, BlendOp blendOp = BLEND_OP_ADD, BlendFactor srcBlendAlpha = BLEND_ONE, BlendFactor destBlendAlpha = BLEND_ONE, BlendOp blendOpAlpha = BLEND_OP_ADD, unsigned char colorWriteMask = COLORMASK_ALL, bool alphaToCoverage = false);

    /// Source color blend factor.
    BlendFactor srcBlend;
    /// Destination color blend factor.
    BlendFactor destBlend;
    /// Color blend operation.
    BlendOp blendOp;
    /// Source alpha blend factor.
    BlendFactor srcBlendAlpha;
    /// Destination alpha blend factor.
    BlendFactor destBlendAlpha;
    /// Color blend operation.
    BlendOp blendOpAlpha;
    /// Rendertarget color write mask.
    unsigned char colorWriteMask;
    /// Blend enable flag.
    bool blendEnable;
    /// Alpha to coverage flag.
    bool alphaToCoverage;
};

}