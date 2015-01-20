// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Matrix3x4.h"
#include "GraphicsDefs.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

extern TURSO3D_API const size_t elementSizes[] =
{
    sizeof(int),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(unsigned),
    sizeof(Matrix3x4),
    sizeof(Matrix4)
};

extern TURSO3D_API const char* elementSemanticNames[] =
{
    "POSITION",
    "NORMAL",
    "BINORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    nullptr
};

extern TURSO3D_API const char* resourceUsageNames[] =
{
    "default",
    "immutable",
    "dynamic",
    "rendertarget",
    nullptr
};

extern TURSO3D_API const char* elementTypeNames[] =
{
    "int",
    "float",
    "Vector2",
    "Vector3",
    "Vector4",
    "UByte4",
    "Matrix3x4",
    "Matrix4",
    nullptr
};

extern TURSO3D_API const char* blendFactorNames[] =
{
    "",
    "zero",
    "one",
    "srcColor",
    "invSrcColor",
    "srcAlpha",
    "invSrcAlpha",
    "destAlpha",
    "invDestAlpha",
    "destColor",
    "invDestColor",
    "srcAlphaSat",
    nullptr
};

extern TURSO3D_API const char* blendOpNames[] =
{
    "",
    "add",
    "subtract",
    "revSubtract",
    "min",
    "max",
    nullptr
};

extern TURSO3D_API const char* blendModeNames[] =
{
    "replace",
    "add",
    "multiply",
    "alpha",
    "addAlpha",
    "preMulAlpha",
    "invDestAlpha",
    "subtract",
    "subtractAlpha",
    nullptr
};

extern TURSO3D_API const char* fillModeNames[] =
{
    "",
    "",
    "wireframe",
    "solid",
    nullptr
};

extern TURSO3D_API const char* cullModeNames[] =
{
    "",
    "none",
    "front",
    "back",
    nullptr
};

extern TURSO3D_API const char* compareFuncNames[] =
{
    "",
    "never",
    "less",
    "equal",
    "lessEqual",
    "greater",
    "notEqual",
    "greaterEqual",
    "always",
    nullptr
};

extern TURSO3D_API const char* stencilOpNames[] =
{
    "",
    "keep",
    "zero",
    "replace",
    "incrSat",
    "descrSat",
    "invert",
    "incr",
    "decr",
    nullptr
};

extern TURSO3D_API const BlendModeDesc blendModes[] =
{
    BlendModeDesc(false, BLEND_ONE, BLEND_ONE, BLEND_OP_ADD, BLEND_ONE, BLEND_ONE, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_ONE, BLEND_ONE, BLEND_OP_ADD, BLEND_ONE, BLEND_ONE, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_DEST_COLOR, BLEND_ZERO, BLEND_OP_ADD, BLEND_DEST_COLOR, BLEND_ZERO, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA, BLEND_OP_ADD, BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_SRC_ALPHA, BLEND_ONE, BLEND_OP_ADD, BLEND_SRC_ALPHA, BLEND_ONE, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_ONE, BLEND_INV_SRC_ALPHA, BLEND_OP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_INV_DEST_ALPHA, BLEND_DEST_ALPHA, BLEND_OP_ADD, BLEND_INV_DEST_ALPHA, BLEND_DEST_ALPHA, BLEND_OP_ADD),
    BlendModeDesc(true, BLEND_ONE, BLEND_ONE, BLEND_OP_REV_SUBTRACT, BLEND_ONE, BLEND_ONE, BLEND_OP_REV_SUBTRACT),
    BlendModeDesc(true, BLEND_SRC_ALPHA, BLEND_ONE, BLEND_OP_REV_SUBTRACT, BLEND_SRC_ALPHA, BLEND_ONE, BLEND_OP_REV_SUBTRACT)
};

}
