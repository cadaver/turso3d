// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

namespace Turso3D
{

/// Clear rendertarget color.
static const unsigned CLEAR_COLOR = 1;
/// Clear rendertarget depth.
static const unsigned CLEAR_DEPTH = 2;
/// Clear rendertarget stencil.
static const unsigned CLEAR_STENCIL = 4;
/// Clear color+depth+stencil.
static const unsigned CLEAR_ALL = 7;

/// Vertex elements.
enum VertexElement
{
    ELEMENT_POSITION = 0,
    ELEMENT_NORMAL,
    ELEMENT_COLOR,
    ELEMENT_TEXCOORD1,
    ELEMENT_TEXCOORD2,
    ELEMENT_CUBETEXCOORD1,
    ELEMENT_CUBETEXCOORD2,
    ELEMENT_TANGENT,
    ELEMENT_BLENDWEIGHTS,
    ELEMENT_BLENDINDICES,
    ELEMENT_INSTANCEMATRIX1,
    ELEMENT_INSTANCEMATRIX2,
    ELEMENT_INSTANCEMATRIX3,
    MAX_VERTEX_ELEMENTS
};

/// Shader stages.
enum ShaderStage
{
    SHADER_VS = 0,
    SHADER_PS,
    MAX_SHADER_STAGES
};

/// Constant types in constant buffers.
enum ConstantType
{
    C_INT = 0,
    C_FLOAT,
    C_VECTOR2,
    C_VECTOR3,
    C_VECTOR4,
    C_COLOR,
    C_MATRIX3X4,
    C_MATRIX4
};

/// Primitive types.
enum PrimitiveType
{
    POINT_LIST = 1,
    LINE_LIST,
    LINE_STRIP,
    TRIANGLE_LIST,
    TRIANGLE_STRIP,
    MAX_PRIMITIVE_TYPES
};

/// Blend factors.
enum BlendFactor
{
    BLEND_ZERO = 1,
    BLEND_ONE,
    BLEND_SRC_COLOR,
    BLEND_INV_SRC_COLOR,
    BLEND_SRC_ALPHA,
    BLEND_INV_SRC_ALPHA,
    BLEND_DEST_ALPHA,
    BLEND_INV_DEST_ALPHA,
    BLEND_DEST_COLOR,
    BLEND_INV_DEST_COLOR,
    BLEND_SRC_ALPHA_SAT,
    BLEND_BLEND_FACTOR,
    BLEND_INV_BLEND_FACTOR,
    BLEND_SRC1_COLOR,
    BLEND_INV_SRC1_COLOR,
    BLEND_SRC1_ALPHA,
    BLEND_INV_SRC1_ALPHA
};

/// Blend operations.
enum BlendOp
{
    BLEND_OP_ADD = 1,
    BLEND_OP_SUBTRACT,
    BLEND_OP_REV_SUBTRACT,
    BLEND_OP_MIN,
    BLEND_OP_MAX
};

/// Fill modes.
enum FillMode
{
    FILL_WIREFRAME = 2,
    FILL_SOLID = 3
};

/// Triangle culling modes.
enum CullMode
{
    CULL_NONE = 1,
    CULL_FRONT,
    CULL_BACK
};

/// Depth or stencil compare modes.
enum CompareMode
{
    CMP_NEVER = 1,
    CMP_LESS,
    CMP_EQUAL,
    CMP_LESS_EQUAL,
    CMP_GREATER,
    CMP_NOT_EQUAL,
    CMP_GREATER_EQUAL,
    CMP_ALWAYS
};

/// Stencil operations.
enum StencilOp
{
    STENCIL_OP_KEEP = 1,
    STENCIL_OP_ZERO,
    STENCIL_OP_REPLACE,
    STENCIL_OP_INCR_SAT,
    STENCIL_OP_DECR_SAT,
    STENCIL_OP_INVERT,
    STENCIL_OP_INCR,
    STENCIL_OP_DECR
};

static const unsigned MASK_POSITION = 0x1;
static const unsigned MASK_NORMAL = 0x2;
static const unsigned MASK_COLOR = 0x4;
static const unsigned MASK_TEXCOORD1 = 0x8;
static const unsigned MASK_TEXCOORD2 = 0x10;
static const unsigned MASK_CUBETEXCOORD1 = 0x20;
static const unsigned MASK_CUBETEXCOORD2 = 0x40;
static const unsigned MASK_TANGENT = 0x80;
static const unsigned MASK_BLENDWEIGHTS = 0x100;
static const unsigned MASK_BLENDINDICES = 0x200;
static const unsigned MASK_INSTANCEMATRIX1 = 0x400;
static const unsigned MASK_INSTANCEMATRIX2 = 0x800;
static const unsigned MASK_INSTANCEMATRIX3 = 0x1000;

static const size_t MAX_VERTEX_STREAMS = 4;
static const size_t MAX_CONSTANT_BUFFERS = 15;

static const unsigned char COLORMASK_NONE = 0x0;
static const unsigned char COLORMASK_R = 0x1;
static const unsigned char COLORMASK_G = 0x2;
static const unsigned char COLORMASK_B = 0x4;
static const unsigned char COLORMASK_A = 0x8;
static const unsigned char COLORMASK_ALL = 0xf;

}
