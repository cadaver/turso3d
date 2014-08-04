// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"
#include "../Base/String.h"

#include <cstdlib>

namespace Turso3D
{

/// Exit the application with an error message to the console.
TURSO3D_API void ErrorExit(const String& message = String::EMPTY, int exitCode = EXIT_FAILURE);
/// Open a console window.
TURSO3D_API void OpenConsoleWindow();
/// Print Unicode text to the console. Will not be printed to the MSVC output window.
TURSO3D_API void PrintUnicode(const String& str, bool error = false);
/// Print Unicode text to the console with a newline appended. Will not be printed to the MSVC output window.
TURSO3D_API void PrintUnicodeLine(const String& str, bool error = false);
/// Print ASCII text to the console with a newline appended. Uses printf() to allow printing into the MSVC output window.
TURSO3D_API void PrintLine(const String& str, bool error = false);
/// Read input from the console window. Return empty if no input.
TURSO3D_API String ReadLine();

}
