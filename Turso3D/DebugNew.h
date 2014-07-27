// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#if defined(_MSC_VER) && defined(_DEBUG)

#define _CRTDBG_MAP_ALLOC

#ifdef _malloca
#undef _malloca
#endif

#include <crtdbg.h>

#define _DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new _DEBUG_NEW

#endif
