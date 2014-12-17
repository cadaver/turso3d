// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

class JSONValue;

/// Description of how to depth & stencil test fragments.
class TURSO3D_API DepthState : public RefCounted, public GPUObject
{
public:
    /// Construct.
    DepthState();
    /// Destruct.
    ~DepthState();

    /// Release the depth state object.
    void Release() override;

    /// Load from JSON data. Return true on success.
    bool LoadJSON(const JSONValue& source);
    /// Save as JSON data.
    void SaveJSON(JSONValue& dest);
    /// Define parameters and create the depth state object. The existing state object (if any) will be destroyed. Return true on success.
    bool Define(bool depthEnable = true, bool depthWrite = true, CompareFunc depthFunc = CMP_LESS, bool stencilEnable = false, unsigned char stencilReadMask = 0xff, unsigned char stencilWriteMask = 0xff, StencilOp frontFail = STENCIL_OP_KEEP, StencilOp frontDepthFail = STENCIL_OP_KEEP, StencilOp frontPass = STENCIL_OP_KEEP, CompareFunc frontFunc = CMP_ALWAYS, StencilOp backFail = STENCIL_OP_KEEP, StencilOp backDepthFail = STENCIL_OP_KEEP, StencilOp backPass = STENCIL_OP_KEEP, CompareFunc backFunc = CMP_ALWAYS);

    /// Depth enable flag.
    bool depthEnable;
    /// Depth write flag.
    bool depthWrite;
    /// Depth testing function.
    CompareFunc depthFunc;
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
    CompareFunc frontFunc;
    /// Stencil operation on back face fail.
    StencilOp backFail;
    /// Stencil operation on back face depth fail.
    StencilOp backDepthFail;
    /// Stencil operation on back face pass.
    StencilOp backPass;
    /// Stencil back face testing function.
    CompareFunc backFunc;
};

}