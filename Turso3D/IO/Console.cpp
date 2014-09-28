// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/WString.h"
#include "Console.h"

#include <cstdio>
#include <fcntl.h>

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "../Debug/DebugNew.h"

namespace Turso3D
{

#ifdef WIN32
static bool consoleOpened = false;
#endif
static String currentLine;

void ErrorExit(const String& message, int exitCode)
{
    if (!message.IsEmpty())
        PrintLine(message, true);

    exit(exitCode);
}

void OpenConsoleWindow()
{
    #ifdef WIN32
    if (consoleOpened)
        return;

    AllocConsole();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((size_t)hOut, _O_TEXT);
    FILE* outFile = _fdopen(hCrt, "w");
    setvbuf(outFile, 0, _IONBF, 1);
    *stdout = *outFile;

    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((size_t)hIn, _O_TEXT);
    FILE* inFile = _fdopen(hCrt, "r");
    setvbuf(inFile, 0, _IONBF, 128);
    *stdin = *inFile;

    consoleOpened = true;
    #endif
}

void PrintUnicode(const String& str, bool error)
{
    #if !defined(ANDROID) && !defined(IOS)
    #ifdef WIN32
    HANDLE stream = GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (stream == INVALID_HANDLE_VALUE)
        return;
    WString strW(str);
    DWORD charsWritten;
    WriteConsoleW(stream, strW.CString(), (DWORD)strW.Length(), &charsWritten, 0);
    #else
    fprintf(error ? stderr : stdout, "%s", str.CString());
    #endif
    #endif
}

void PrintUnicodeLine(const String& str, bool error)
{
    PrintUnicode(str + '\n', error);
}

void PrintLine(const String& str, bool error)
{
    #if !defined(ANDROID) && !defined(IOS)
    fprintf(error ? stderr: stdout, "%s\n", str.CString());
    #endif
}

String ReadLine()
{
    String ret;
    
    #ifdef WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (input == INVALID_HANDLE_VALUE || output == INVALID_HANDLE_VALUE)
        return ret;

    // Use char-based input
    SetConsoleMode(input, ENABLE_PROCESSED_INPUT);

    INPUT_RECORD record;
    DWORD events = 0;
    DWORD readEvents = 0;

    if (!GetNumberOfConsoleInputEvents(input, &events))
        return ret;

    while (events--)
    {
        ReadConsoleInputW(input, &record, 1, &readEvents);
        if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
        {
            unsigned c = record.Event.KeyEvent.uChar.UnicodeChar;
            if (c)
            {
                if (c == '\b')
                {
                    PrintUnicode("\b \b");
                    size_t length = currentLine.LengthUTF8();
                    if (length)
                        currentLine = currentLine.SubstringUTF8(0, length - 1);
                }
                else if (c == '\r')
                {
                    PrintUnicode("\n");
                    ret = currentLine;
                    currentLine.Clear();
                    return ret;
                }
                else
                {
                    // We have disabled echo, so echo manually
                    wchar_t out = (wchar_t)c;
                    DWORD charsWritten;
                    WriteConsoleW(output, &out, 1, &charsWritten, 0);
                    currentLine.AppendUTF8(c);
                }
            }
        }
    }
    #else
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    for (;;)
    {
        int ch = fgetc(stdin);
        if (ch >= 0 && ch != '\n')
            ret += (char)ch;
        else
            break;
    }
    #endif

    return ret;
}

}
