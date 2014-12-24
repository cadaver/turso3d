// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

#ifdef TURSO3D_D3D11
    #include "D3D11/D3D11IndexBuffer.h"
#endif
#ifdef TURSO3D_OPENGL
    #include "GL/GLIndexBuffer.h"
#endif