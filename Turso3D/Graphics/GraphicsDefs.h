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

/// Shader stages.
enum ShaderStage
{
    SHADER_VS = 0,
    SHADER_PS,
    MAX_SHADER_STAGES
};

/// Element types for constant buffers and vertex elements.
enum ElementType
{
    ELEM_INT = 0,
    ELEM_FLOAT,
    ELEM_VECTOR2,
    ELEM_VECTOR3,
    ELEM_VECTOR4,
    ELEM_UBYTE4,
    ELEM_MATRIX3X4,
    ELEM_MATRIX4,
    NUM_ELEMENT_TYPES
};

/// Element semantics for vertex elements.
enum ElementSemantic
{
    SEM_POSITION = 0,
    SEM_NORMAL,
    SEM_BINORMAL,
    SEM_TANGENT,
    SEM_TEXCOORD,
    SEM_COLOR,
    SEM_BLENDWEIGHT,
    SEM_BLENDINDICES,
    MAX_ELEMENT_SEMANTICS
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

/// Texture types.
enum TextureType
{
    TEX_1D = 0,
    TEX_2D,
    TEX_3D,
    TEX_CUBE,
};

/// Resource usage modes. Rendertarget usage can only be used with textures.
enum ResourceUsage
{
    USAGE_DEFAULT = 0,
    USAGE_IMMUTABLE,
    USAGE_DYNAMIC,
    USAGE_RENDERTARGET
};

/// Texture filtering modes.
enum TextureFilterMode
{
    FILTER_POINT = 0,
    FILTER_BILINEAR,
    FILTER_TRILINEAR,
    FILTER_ANISOTROPIC,
};

/// Texture addressing modes.
enum TextureAddressMode
{
    ADDRESS_WRAP = 1,
    ADDRESS_MIRROR,
    ADDRESS_CLAMP,
    ADDRESS_BORDER,
    ADDRESS_MIRROR_ONCE
};

/// Description of an element in a vertex declaration.
struct TURSO3D_API VertexElement
{
    /// Default-construct.
    VertexElement() :
        type(ELEM_VECTOR3),
        semantic(SEM_POSITION),
        index(0),
        perInstance(false),
        offset(0)
    {
    }

    /// Construct with type, semantic, index and whether is per-instance data.
    VertexElement(ElementType type_, ElementSemantic semantic_, unsigned char index_ = 0, bool perInstance_ = false) :
        type(type_),
        semantic(semantic_),
        index(index_),
        perInstance(perInstance_),
        offset(0)
    {
    }

    /// Data type of element.
    ElementType type;
    /// Semantic of element.
    ElementSemantic semantic;
    /// Semantic index of element, for example multi-texcoords.
    unsigned char index;
    /// Per-instance flag.
    bool perInstance;
    /// Offset of element from vertex start. Filled by VertexBuffer.
    size_t offset;
};

/// Description of a shader constant.
struct TURSO3D_API Constant
{
    /// Construct empty.
    Constant() :
        numElements(1)
    {
    }

    /// Construct with type, name and optional number of elements.
    Constant(ElementType type_, const String& name_, size_t numElements_ = 1) :
        type(type_),
        name(name_),
        numElements(numElements_)
    {
    }

    /// Construct with type, name and optional number of elements.
    Constant(ElementType type_, const char* name_, size_t numElements_ = 1) :
        type(type_),
        name(name_),
        numElements(numElements_)
    {
    }

    /// Data type of constant.
    ElementType type;
    /// Name of constant.
    String name;
    /// Number of elements. Default 1.
    size_t numElements;
    /// Element size. Filled by ConstantBuffer.
    size_t elementSize;
    /// Offset from the beginning of the buffer. Filled by ConstantBuffer.
    size_t offset;
};

static const size_t MAX_VERTEX_STREAMS = 4;
static const size_t MAX_CONSTANT_BUFFERS = 15;
static const size_t MAX_TEXTURE_UNITS = 16;
static const size_t MAX_RENDERTARGETS = 4;

static const unsigned char COLORMASK_NONE = 0x0;
static const unsigned char COLORMASK_R = 0x1;
static const unsigned char COLORMASK_G = 0x2;
static const unsigned char COLORMASK_B = 0x4;
static const unsigned char COLORMASK_A = 0x8;
static const unsigned char COLORMASK_ALL = 0xf;

}
