// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Console.h"
#include "../IO/File.h"
#include "../Thread/Thread.h"
#include "../Thread/Timer.h"
#include "Log.h"

#include <cstdio>
#include <ctime>

#include "DebugNew.h"

namespace Turso3D
{

const char* logLevelPrefixes[] =
{
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    nullptr
};

Log::Log() :
#ifdef _DEBUG
    level(LOG_DEBUG),
#else
    level(LOG_INFO),
#endif
    timeStamp(false),
    inWrite(false),
    quiet(false)
{
    RegisterSubsystem(this);
}

Log::~Log()
{
    Close();
    RemoveSubsystem(this);
}

void Log::Open(const String& fileName)
{
    if (fileName.IsEmpty())
        return;
    
    if (logFile && logFile->IsOpen())
    {
        if (logFile->Name() == fileName)
            return;
        else
            Close();
    }

    logFile = new File();
    if (logFile->Open(fileName, FILE_WRITE))
        LOGINFO("Opened log file " + fileName);
    else
    {
        logFile.Reset();
        LOGERROR("Failed to create log file " + fileName);
    }
}

void Log::Close()
{
    if (logFile && logFile->IsOpen())
    {
        logFile->Close();
        logFile.Reset();
    }
}

void Log::SetLevel(int newLevel)
{
    assert(newLevel >= LOG_DEBUG && newLevel < LOG_NONE);

    level = newLevel;
}

void Log::SetTimeStamp(bool enable)
{
    timeStamp = enable;
}

void Log::SetQuiet(bool enable)
{
    quiet = enable;
}

void Log::EndFrame()
{
    MutexLock lock(logMutex);

    // Process messages accumulated from other threads (if any)
    while (!threadMessages.IsEmpty())
    {
        const StoredLogMessage& stored = threadMessages.Front();

        if (stored.level != LOG_RAW)
            Write(stored.level, stored.message);
        else
            WriteRaw(stored.message, stored.error);

        threadMessages.PopFront();
    }
}

void Log::Write(int msgLevel, const String& message)
{
    assert(msgLevel >= LOG_DEBUG && msgLevel < LOG_NONE);
    
    Log* instance = Subsystem<Log>();
    if (!instance)
        return;

    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        MutexLock lock(instance->logMutex);
        instance->threadMessages.Push(StoredLogMessage(message, msgLevel, false));
        return;
    }

    // Do not log if message level excluded or if currently sending a log event
    if (instance->level > msgLevel || instance->inWrite)
        return;

    String formattedMessage = logLevelPrefixes[msgLevel];
    formattedMessage += ": " + message;
    instance->lastMessage = message;

    if (instance->timeStamp)
        formattedMessage = "[" + TimeStamp() + "] " + formattedMessage;
    
    if (instance->quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (msgLevel == LOG_ERROR)
            PrintUnicodeLine(formattedMessage, true);
    }
    else
        PrintUnicodeLine(formattedMessage, msgLevel == LOG_ERROR);

    if (instance->logFile)
    {
        instance->logFile->WriteLine(formattedMessage);
        instance->logFile->Flush();
    }

    instance->inWrite = true;

    LogMessageEvent& event = instance->logMessageEvent;
    event.message = formattedMessage;
    event.level = msgLevel;
    instance->SendEvent(event);

    instance->inWrite = false;
}

void Log::WriteRaw(const String& message, bool error)
{
    Log* instance = Subsystem<Log>();
    if (!instance)
        return;

    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        MutexLock lock(instance->logMutex);
        instance->threadMessages.Push(StoredLogMessage(message, LOG_RAW, error));
        return;
    }
    
    // Prevent recursion during log event
    if (instance->inWrite)
        return;

    instance->lastMessage = message;

    if (instance->quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (error)
            PrintUnicode(message, true);
    }
    else
        PrintUnicode(message, error);

    if (instance->logFile)
    {
        instance->logFile->Write(message.CString(), message.Length());
        instance->logFile->Flush();
    }

    instance->inWrite = true;

    LogMessageEvent& event = instance->logMessageEvent;
    event.message = message;
    event.level = error ? LOG_ERROR : LOG_INFO;
    instance->SendEvent(event);

    instance->inWrite = false;
}

}
