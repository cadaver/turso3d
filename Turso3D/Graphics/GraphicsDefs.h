// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"
#include "../Base/String.h"
#include "../Math/IntRect.h"

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

/// Maximum simultaneous vertex buffers.
static const size_t MAX_VERTEX_STREAMS = 4;
/// Maximum simultaneous constant buffers.
static const size_t MAX_CONSTANT_BUFFERS = 15;
/// Maximum number of textures in use at once.
static const size_t MAX_TEXTURE_UNITS = 16;
/// Maximum number of textures reserved for materials, starting from 0.
static const size_t MAX_MATERIAL_TEXTURE_UNITS = 8;
/// Maximum number of color rendertargets in use at once.
static const size_t MAX_RENDERTARGETS = 4;

/// Disable color write.
static const unsigned char COLORMASK_NONE = 0x0;
/// Write to red channel.
static const unsigned char COLORMASK_R = 0x1;
/// Write to green channel.
static const unsigned char COLORMASK_G = 0x2;
/// Write to blue channel.
static const unsigned char COLORMASK_B = 0x4;
/// Write to alpha channel.
static const unsigned char COLORMASK_A = 0x8;
/// Write to all color channels (default.)
static const unsigned char COLORMASK_ALL = 0xf;

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
    MAX_ELEMENT_TYPES
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
    MAX_BLEND_FACTORS
};

/// Blend operations.
enum BlendOp
{
    BLEND_OP_ADD = 1,
    BLEND_OP_SUBTRACT,
    BLEND_OP_REV_SUBTRACT,
    BLEND_OP_MIN,
    BLEND_OP_MAX,
    MAX_BLEND_OPS
};

/// Predefined blend modes.
enum BlendMode
{
    BLEND_MODE_REPLACE = 0,
    BLEND_MODE_ADD,
    BLEND_MODE_MULTIPLY,
    BLEND_MODE_ALPHA,
    BLEND_MODE_ADDALPHA,
    BLEND_MODE_PREMULALPHA,
    BLEND_MODE_INVDESTALPHA,
    BLEND_MODE_SUBTRACT,
    BLEND_MODE_SUBTRACTALPHA,
    MAX_BLEND_MODES
};

/// Fill modes.
enum FillMode
{
    FILL_WIREFRAME = 2,
    FILL_SOLID = 3,
    MAX_FILL_MODES
};

/// Triangle culling modes.
enum CullMode
{
    CULL_NONE = 1,
    CULL_FRONT,
    CULL_BACK,
    MAX_CULL_MODES
};

/// Depth or stencil compare modes.
enum CompareFunc
{
    CMP_NEVER = 1,
    CMP_LESS,
    CMP_EQUAL,
    CMP_LESS_EQUAL,
    CMP_GREATER,
    CMP_NOT_EQUAL,
    CMP_GREATER_EQUAL,
    CMP_ALWAYS,
    MAX_COMPARE_MODES
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
    STENCIL_OP_DECR,
    MAX_STENCIL_OPS
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

/// Description of a blend mode.
struct TURSO3D_API BlendModeDesc
{
    /// Default-construct.
    BlendModeDesc()
    {
        Reset();
    }

    /// Construct with parameters.
    BlendModeDesc(bool blendEnable_, BlendFactor srcBlend_, BlendFactor destBlend_, BlendOp blendOp_, BlendFactor srcBlendAlpha_, BlendFactor destBlendAlpha_, BlendOp blendOpAlpha_) :
        blendEnable(blendEnable_),
        srcBlend(srcBlend_),
        destBlend(destBlend_),
        blendOp(blendOp_),
        srcBlendAlpha(srcBlendAlpha_),
        destBlendAlpha(destBlendAlpha_),
        blendOpAlpha(blendOpAlpha_)
    {
    }

    /// Reset to defaults.
    void Reset()
    {
        blendEnable = false;
        srcBlend = BLEND_ONE;
        destBlend = BLEND_ONE;
        blendOp = BLEND_OP_ADD;
        srcBlendAlpha = BLEND_ONE;
        destBlendAlpha = BLEND_ONE;
        blendOpAlpha = BLEND_OP_ADD;
    }

    /// Test for equality with another blend mode description.
    bool operator == (const BlendModeDesc& rhs) const { return blendEnable == rhs.blendEnable && srcBlend == rhs.srcBlend && destBlend == rhs.destBlend && blendOp == rhs.blendOp && srcBlendAlpha == rhs.srcBlendAlpha && destBlendAlpha == rhs.destBlendAlpha && blendOpAlpha == rhs.blendOpAlpha; }
    /// Test for inequality with another blend mode description.
    bool operator != (const BlendModeDesc& rhs) const { return !(*this == rhs); }

    /// Blend enable flag.
    bool blendEnable;
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
    /// Alpha blend operation.
    BlendOp blendOpAlpha;
};

/// Description of a stencil test.
struct TURSO3D_API StencilTestDesc
{
    /// Default-construct.
    StencilTestDesc()
    {
        Reset();
    }

    /// Reset to defaults.
    void Reset()
    {
        stencilReadMask = 0xff;
        stencilWriteMask = 0xff;
        frontFunc = CMP_ALWAYS;
        frontFail = STENCIL_OP_KEEP;
        frontDepthFail = STENCIL_OP_KEEP;
        frontPass = STENCIL_OP_KEEP;
        backFunc = CMP_ALWAYS;
        backFail = STENCIL_OP_KEEP;
        backDepthFail = STENCIL_OP_KEEP;
        backPass = STENCIL_OP_KEEP;
    }

    /// Stencil read bit mask.
    unsigned char stencilReadMask;
    /// Stencil write bit mask.
    unsigned char stencilWriteMask;
    /// Stencil front face compare function.
    CompareFunc frontFunc;
    /// Operation for front face stencil test fail.
    StencilOp frontFail;
    /// Operation for front face depth test fail.
    StencilOp frontDepthFail;
    /// Operation for front face pass.
    StencilOp frontPass;
    /// Stencil back face compare function.
    CompareFunc backFunc;
    /// Operation for back face stencil test fail.
    StencilOp backFail;
    /// Operation for back face depth test fail.
    StencilOp backDepthFail;
    /// Operation for back face pass.
    StencilOp backPass;
};

/// Collection of render state.
struct RenderState
{
    /// Default-construct.
    RenderState()
    {
        Reset();
    }

    /// Reset to defaults.
    void Reset()
    {
        depthFunc = CMP_LESS_EQUAL;
        depthWrite = true;
        depthClip = true;
        depthBias = 0;
        depthBiasClamp = M_INFINITY;
        slopeScaledDepthBias = 0.0f;
        colorWriteMask = COLORMASK_ALL;
        alphaToCoverage = false;
        blendMode.Reset();
        cullMode = CULL_BACK;
        fillMode = FILL_SOLID;
        scissorEnable = false;
        scissorRect = IntRect::ZERO;
        stencilEnable = false;
        stencilRef = 0;
        stencilTest.Reset();
    }

    /// Depth test function.
    CompareFunc depthFunc;
    /// Depth write enable.
    bool depthWrite;
    /// Depth clipping enable.
    bool depthClip;
    /// Constant depth bias.
    int depthBias;
    /// Maximum allowed depth bias.
    float depthBiasClamp;
    /// Slope-scaled depth bias.
    float slopeScaledDepthBias;
    /// Rendertarget color channel write mask.
    unsigned char colorWriteMask;
    /// Alpha-to-coverage enable.
    bool alphaToCoverage;
    /// Blend mode parameters.
    BlendModeDesc blendMode;
    /// Polygon culling mode.
    CullMode cullMode;
    /// Polygon fill mode.
    FillMode fillMode;
    /// Scissor test enable.
    bool scissorEnable;
    /// Scissor rectangle as pixels from rendertarget top left corner.
    IntRect scissorRect;
    /// Stencil test enable.
    bool stencilEnable;
    /// Stencil reference value.
    unsigned char stencilRef;
    /// Stencil test parameters.
    StencilTestDesc stencilTest;
};

/// Vertex element sizes by element type.
extern TURSO3D_API const size_t elementSizes[];
/// Resource usage names.
extern TURSO3D_API const char* resourceUsageNames[];
/// Element type names.
extern TURSO3D_API const char* elementTypeNames[];
/// Vertex element semantic names.
extern TURSO3D_API const char* elementSemanticNames[];
/// Blend factor names.
extern TURSO3D_API const char* blendFactorNames[];
/// Blend operation names.
extern TURSO3D_API const char* blendOpNames[];
/// Predefined blend mode names.
extern TURSO3D_API const char* blendModeNames[];
/// Fill mode names.
extern TURSO3D_API const char* fillModeNames[];
/// Culling mode names.
extern TURSO3D_API const char* cullModeNames[];
/// Compare function names.
extern TURSO3D_API const char* compareFuncNames[];
/// Stencil operation names.
extern TURSO3D_API const char* stencilOpNames[];
/// Predefined blend modes.
extern TURSO3D_API const BlendModeDesc blendModes[];

}
