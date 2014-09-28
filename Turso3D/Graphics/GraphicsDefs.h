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
    SHADER_VS,
    SHADER_PS
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

}
