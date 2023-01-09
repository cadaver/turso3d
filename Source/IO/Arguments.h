// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include <string>
#include <vector>

/// Parse arguments from the command line. First argument is by default assumed to be the executable name and is skipped.
const std::vector<std::string>& ParseArguments(const std::string& cmdLine);
/// Parse arguments from the command line.
const std::vector<std::string>& ParseArguments(const char* cmdLine);
/// Parse arguments from argc & argv.
const std::vector<std::string>& ParseArguments(int argc, char** argv);
/// Return previously parsed arguments.
const std::vector<std::string>& Arguments();
