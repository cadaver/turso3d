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

}