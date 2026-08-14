// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

// On web build, include GLES3 headers. On desktop, include OpenGL + extensions through GLEW library.
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#else
#include <glew.h>
#endif
