// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <cstddef>

/// Maximum simultaneous vertex buffers.
static const size_t MAX_VERTEX_STREAMS = 4;
/// Maximum number of material textures
static const size_t MAX_MATERIAL_TEXTURE_UNITS = 8;
/// Maximum number of textures in use at once.
static const size_t MAX_TEXTURE_UNITS = 16;
/// Maximum number of constant buffer slots in use at once.
static const size_t MAX_CONSTANT_BUFFER_SLOTS = 8;
/// Maximum number of color rendertargets in use at once.
static const size_t MAX_RENDERTARGETS = 4;
/// Number of cube map faces.
static const size_t MAX_CUBE_FACES = 6;

/// Primitive types.
enum PrimitiveType
{
    PT_LINE_LIST = 0,
    PT_TRIANGLE_LIST
};

/// Element types for vertex elements.
enum ElementType
{
    ELEM_INT = 0,
    ELEM_FLOAT,
    ELEM_VECTOR2,
    ELEM_VECTOR3,
    ELEM_VECTOR4,
    ELEM_UBYTE4,
    MAX_ELEMENT_TYPES
};

/// Element semantics for vertex elements.
enum ElementSemantic
{
    SEM_POSITION = 0,
    SEM_NORMAL,
    SEM_TANGENT,
    SEM_COLOR,
    SEM_TEXCOORD,
    SEM_BLENDWEIGHTS,
    SEM_BLENDINDICES,
    MAX_ELEMENT_SEMANTICS
};

/// Vertex attribute indices.
enum VertexAttribute
{
    ATTR_POSITION = 0,
    ATTR_NORMAL,
    ATTR_TANGENT,
    ATTR_VERTEXCOLOR,
    ATTR_TEXCOORD,
    ATTR_TEXCOORD1,
    ATTR_TEXCOORD2,
    ATTR_TEXCOORD3,
    ATTR_TEXCOORD4,
    ATTR_TEXCOORD5,
    ATTR_BLENDWEIGHTS,
    ATTR_BLENDINDICES,
    MAX_VERTEX_ATTRIBUTES
};

/// Vertex attribute bitmasks.
enum AttributeMask
{
    MASK_POSITION = 1 << ATTR_POSITION,
    MASK_NORMAL = 1 << ATTR_NORMAL,
    MASK_TANGENT = 1 << ATTR_TANGENT,
    MASK_VERTEXCOLOR = 1 << ATTR_VERTEXCOLOR,
    MASK_TEXCOORD = 1 << ATTR_TEXCOORD,
    MASK_TEXCOORD1 = 1 << ATTR_TEXCOORD1,
    MASK_TEXCOORD2 = 1 << ATTR_TEXCOORD2,
    MASK_TEXCOORD3 = 1 << ATTR_TEXCOORD3,
    MASK_TEXCOORD4 = 1 << ATTR_TEXCOORD4,
    MASK_TEXCOORD5 = 1 << ATTR_TEXCOORD5,
    MASK_BLENDWEIGHTS = 1 << ATTR_BLENDWEIGHTS,
    MASK_BLENDINDICES = 1 << ATTR_BLENDINDICES
};

/// Predefined blend modes.
enum BlendMode
{
    BLEND_REPLACE = 0,
    BLEND_ADD,
    BLEND_MULTIPLY,
    BLEND_ALPHA,
    BLEND_ADDALPHA,
    BLEND_PREMULALPHA,
    BLEND_INVDESTALPHA,
    BLEND_SUBTRACT,
    BLEND_SUBTRACTALPHA,
    MAX_BLEND_MODES
};

/// Triangle culling modes.
enum CullMode
{
    CULL_NONE = 0,
    CULL_FRONT,
    CULL_BACK,
    MAX_CULL_MODES
};

/// Depth or stencil compare modes.
enum CompareMode
{
    CMP_NEVER = 0,
    CMP_LESS,
    CMP_EQUAL,
    CMP_LESS_EQUAL,
    CMP_GREATER,
    CMP_NOT_EQUAL,
    CMP_GREATER_EQUAL,
    CMP_ALWAYS,
    MAX_COMPARE_MODES
};

/// Texture types.
enum TextureType
{
    TEX_2D = 0,
    TEX_3D,
    TEX_CUBE,
};

/// Resource usage modes for buffers.
enum ResourceUsage
{
    USAGE_DEFAULT = 0,
    USAGE_DYNAMIC
};

/// Texture filtering modes.
enum TextureFilterMode
{
    FILTER_POINT = 0,
    FILTER_BILINEAR,
    FILTER_TRILINEAR,
    FILTER_ANISOTROPIC,
    COMPARE_POINT,
    COMPARE_BILINEAR,
    COMPARE_TRILINEAR,
    COMPARE_ANISOTROPIC
};

/// Texture addressing modes.
enum TextureAddressMode
{
    ADDRESS_WRAP = 0,
    ADDRESS_MIRROR,
    ADDRESS_CLAMP,
    ADDRESS_BORDER,
    ADDRESS_MIRROR_ONCE
};

/// Preset uniforms outside uniform buffers.
enum PresetUniform
{
    U_WORLDMATRIX,
    MAX_PRESET_UNIFORMS
};

/// Uniform buffer binding points.
enum UniformBufferBindings
{
    UB_PERVIEWDATA = 0,
    UB_LIGHTDATA,
    UB_OBJECTDATA,
    UB_MATERIALDATA
};

/// Geometry types for vertex shader.
enum GeometryType
{
    GEOM_STATIC = 0,
    GEOM_SKINNED,
    GEOM_INSTANCED,
    GEOM_CUSTOM
};

/// Description of an element in a vertex declaration.
struct VertexElement
{
    /// Default-construct.
    VertexElement() :
        type(ELEM_VECTOR3),
        semantic(SEM_POSITION),
        index(0),
        offset(0)
    {
    }

    /// Construct with type, semantic, index and whether is per-instance data.
    VertexElement(ElementType type_, ElementSemantic semantic_, unsigned char index_ = 0) :
        type(type_),
        semantic(semantic_),
        index(index_),
        offset(0)
    {
    }

    /// Data type of element.
    ElementType type;
    /// Semantic of element.
    ElementSemantic semantic;
    /// Semantic index of element, for example multi-texcoords.
    unsigned char index;
    /// Offset of element from vertex start. Filled by VertexBuffer.
    size_t offset;
};

/// Vertex element sizes by element type.
extern const size_t elementSizes[];
/// Vertex element semantic names.
extern const char* elementSemanticNames[];
/// Preset uniform names.
extern const char* presetUniformNames[];
/// Blend mode names.
extern const char* blendModeNames[];
/// Culling mode names.
extern const char* cullModeNames[];
/// Compare mode names.
extern const char* compareModeNames[];
