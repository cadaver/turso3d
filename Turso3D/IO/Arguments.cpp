// For conditions of distribution and use, see copyright notice in License.txt

#include "Arguments.h"
#include "StringUtils.h"

#include <cstdio>

static std::vector<std::string> arguments;

const std::vector<std::string>& ParseArguments(const std::string& cmdLine)
{
    arguments.clear();

    size_t cmdStart = 0, cmdEnd = 0;
    bool inCmd = false;
    bool inQuote = false;

    for (size_t i = 0; i < cmdLine.length(); ++i)
    {
        if (cmdLine[i] == '\"')
            inQuote = !inQuote;
        if (cmdLine[i] == ' ' && !inQuote)
        {
            if (inCmd)
            {
                inCmd = false;
                cmdEnd = i;
                arguments.push_back(cmdLine.substr(cmdStart, cmdEnd - cmdStart));
            }
        }
        else
        {
            if (!inCmd)
            {
                inCmd = true;
                cmdStart = i;
            }
        }
    }
    if (inCmd)
    {
        cmdEnd = cmdLine.length();
        arguments.push_back(cmdLine.substr(cmdStart, cmdEnd - cmdStart));
    }

    // Strip double quotes from the arguments
    for (size_t i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i].length() >= 2 && StartsWith(arguments[i], "\"") && EndsWith(arguments[i], "\""))
            arguments[i] = arguments[i].substr(1, arguments[i].length() - 2);
    }

    return arguments;
}

const std::vector<std::string>& ParseArguments(const char* cmdLine)
{
    return ParseArguments(std::string(cmdLine));
}

const std::vector<std::string>& ParseArguments(int argc, char** argv)
{
    std::string cmdLine;

    for (int i = 0; i < argc; ++i)
        cmdLine += FormatString("\"%s\" ", argv[i]);

    return ParseArguments(cmdLine);
}

const std::vector<std::string>& Arguments()
{
    return arguments;
}
