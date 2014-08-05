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
    0
};

Log::Log() :
#ifdef _DEBUG
    level(LOG_DEBUG),
#else
    level(LOG_INFO),
#endif
    timeStamp(true),
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
        Write(LOG_INFO, "Opened log file " + fileName);
    else
    {
        logFile.Reset();
        Write(LOG_ERROR, "Failed to create log file " + fileName);
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

void Log::Write(int msgLevel, const String& message)
{
    assert(msgLevel >= LOG_DEBUG && msgLevel < LOG_NONE);

    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        MutexLock lock(logMutex);
        threadMessages.Push(StoredLogMessage(message, msgLevel, false));
        return;
    }

    // Do not log if message level excluded or if currently sending a log event
    if (level > msgLevel || inWrite)
        return;

    String formattedMessage = logLevelPrefixes[msgLevel];
    formattedMessage += ": " + message;
    lastMessage = message;

    if (timeStamp)
        formattedMessage = "[" + TimeStamp() + "] " + formattedMessage;
    
    if (quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (msgLevel == LOG_ERROR)
            PrintUnicodeLine(formattedMessage, true);
    }
    else
        PrintUnicodeLine(formattedMessage, msgLevel == LOG_ERROR);

    if (logFile)
    {
        logFile->WriteLine(formattedMessage);
        logFile->Flush();
    }

    inWrite = true;

    using namespace LogMessage;

    VariantMap eventData;
    eventData[MESSAGE] = formattedMessage;
    eventData[LEVEL] = msgLevel;
    SendEvent(logMessage, eventData);

    inWrite = false;
}

void Log::WriteRaw(const String& message, bool error)
{
    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        MutexLock lock(logMutex);
        threadMessages.Push(StoredLogMessage(message, LOG_RAW, error));
        return;
    }
    
    // Prevent recursion during log event
    if (inWrite)
        return;

    lastMessage = message;

    if (quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (error)
            PrintUnicode(message, true);
    }
    else
        PrintUnicode(message, error);

    if (logFile)
    {
        logFile->Write(message.CString(), message.Length());
        logFile->Flush();
    }

    inWrite = true;

    using namespace LogMessage;

    VariantMap eventData;
    eventData[MESSAGE] = message;
    eventData[LEVEL] = error ? LOG_ERROR : LOG_INFO;
    SendEvent(logMessage, eventData);

    inWrite = false;
}

void Log::ProcessThreadedMessages()
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

void WriteToLog(int msgLevel, const String& message)
{
    Log* logInstance = Subsystem<Log>();
    if (logInstance)
        logInstance->Write(msgLevel, message);
}

void WriteToLogRaw(const String& message, bool error)
{
    Log* logInstance = Subsystem<Log>();
    if (logInstance)
        logInstance->WriteRaw(message, error);
}

}
