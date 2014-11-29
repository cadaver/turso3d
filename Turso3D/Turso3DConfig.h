// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

/// %Turso3D engine namespace.
namespace Turso3D
{
}

// Shared library exports
/* #undef TURSO3D_SHARED */
#if defined(_WIN32) && defined(TURSO3D_SHARED)
#ifdef _MSC_VER
#pragma warning(disable:4251)
#endif
#ifdef TURSO3D_EXPORTS
#define TURSO3D_API __declspec(dllexport)
#else
#define TURSO3D_API __declspec(dllimport)
#endif
#else
#define TURSO3D_API
#endif

// Turso3D build configuration
#define TURSO3D_LOGGING
#define TURSO3D_PROFILING
/* #undef TURSO3D_D3D11 */
#define TURSO3D_OPENGL
