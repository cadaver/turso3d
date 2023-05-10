// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/Matrix3x4.h"
#include "GraphicsDefs.h"

const size_t elementSizes[] =
{
    sizeof(int),
    sizeof(float),
    sizeof(Vector2),
    sizeof(Vector3),
    sizeof(Vector4),
    sizeof(unsigned),
};

const char* elementSemanticNames[] =
{
    "POSITION",
    "NORMAL",
    "TANGENT",
    "TEXCOORD",
    "COLOR",
    "BLENDWEIGHT",
    "BLENDINDICES",
    nullptr
};

const char* presetUniformNames[] = 
{
    "worldMatrix",
    nullptr
};

const char* blendModeNames[] =
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

const char* cullModeNames[] =
{
    "none",
    "front",
    "back",
    nullptr
};

const char* compareModeNames[] =
{
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
