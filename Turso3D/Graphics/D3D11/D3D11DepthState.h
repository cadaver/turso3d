// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

/// Description of how to depth & stencil test fragments.
class TURSO3D_API DepthState : public GPUObject
{
public:
    /// Construct.
    DepthState();
    /// Destruct.
    virtual ~DepthState();

    /// Release the depth state object.
    virtual void Release();

    /// Define parameters and create the depth state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(bool depthEnable = true, bool depthWrite = true, CompareMode depthFunc = CMP_LESS, bool stencilEnable = false, unsigned char stencilReadMask = 0xff, unsigned char stencilWriteMask = 0xff, StencilOp frontFail = STENCIL_OP_KEEP, StencilOp frontDepthFail = STENCIL_OP_KEEP, StencilOp frontPass = STENCIL_OP_KEEP, CompareMode frontFunc = CMP_ALWAYS, StencilOp backFail = STENCIL_OP_KEEP, StencilOp backDepthFail = STENCIL_OP_KEEP, StencilOp backPass = STENCIL_OP_KEEP, CompareMode backFunc = CMP_ALWAYS);
    
    /// Return the D3D11 state object.
    void* StateObject() const { return stateObject; }

    /// Depth enable flag.
    bool depthEnable;
    /// Depth write flag.
    bool depthWrite;
    /// Depth testing function.
    CompareMode depthFunc;
    /// Stencil enable flag.
    bool stencilEnable;
    /// Stencil buffer read mask.
    unsigned char stencilReadMask;
    /// Stencil buffer write mask.
    unsigned char stencilWriteMask;
    /// Stencil operation on front face fail.
    StencilOp frontFail;
    /// Stencil operation on front face depth fail.
    StencilOp frontDepthFail;
    /// Stencil operation on front face pass.
    StencilOp frontPass;
    /// Stencil front face testing function.
    CompareMode frontFunc;
    /// Stencil operation on back face fail.
    StencilOp backFail;
    /// Stencil operation on back face depth fail.
    StencilOp backDepthFail;
    /// Stencil operation on back face pass.
    StencilOp backPass;
    /// Stencil front face testing function.
    CompareMode backFunc;

private:
    /// D3D11 depth state object.
    void* stateObject;
};

}