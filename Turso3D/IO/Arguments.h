// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/String.h"
#include "../Base/Vector.h"
#include "../Base/WString.h"

namespace Turso3D
{

/// Parse arguments from the command line. First argument is by default assumed to be the executable name and is skipped.
TURSO3D_API const Vector<String>& ParseArguments(const String& cmdLine, bool skipFirstArgument = true);
/// Parse arguments from the command line.
TURSO3D_API const Vector<String>& ParseArguments(const char* cmdLine);
/// Parse arguments from a wide char command line.
TURSO3D_API const Vector<String>& ParseArguments(const WString& cmdLine);
/// Parse arguments from a wide char command line.
TURSO3D_API const Vector<String>& ParseArguments(const wchar_t* cmdLine);
/// Parse arguments from argc & argv.
TURSO3D_API const Vector<String>& ParseArguments(int argc, char** argv);
/// Return previously parsed arguments.
TURSO3D_API const Vector<String>& Arguments();

}
