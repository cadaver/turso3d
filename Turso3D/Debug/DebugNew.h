// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

// In MSVC debug mode, override the global new operator to include file and line logging. This file should be included last in
// compilation units. Will break using placement new.

#if defined(_MSC_VER) && defined(_DEBUG)

#define _CRTDBG_MAP_ALLOC

#ifdef _malloca
#undef _malloca
#endif

#include <crtdbg.h>

#define _DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new _DEBUG_NEW

#endif
