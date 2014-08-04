// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Console.h"
#include "../IO/File.h"
#include "../Thread/Thread.h"
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
    logMessage("LogMessage"),
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

    Log* logInstance = Subsystem<Log>();
    
    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        if (logInstance)
        {
            MutexLock lock(logInstance->logMutex);
            logInstance->threadMessages.Push(StoredLogMessage(message, msgLevel, false));
        }
        
        return;
    }

    // Do not log if message level excluded or if currently sending a log event
    if (!logInstance || logInstance->level > msgLevel || logInstance->inWrite)
        return;

    String formattedMessage = logLevelPrefixes[msgLevel];
    formattedMessage += ": " + message;
    logInstance->lastMessage = message;

    if (logInstance->timeStamp)
    {
        /// \todo Move into a function if needed elsewhere
        time_t sysTime;
        time(&sysTime);
        const char* dateTime = ctime(&sysTime);
        formattedMessage = "[" + String(dateTime).Replaced("\n", "") + "] " + formattedMessage;
    }
    
    if (logInstance->quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (msgLevel == LOG_ERROR)
            PrintUnicodeLine(formattedMessage, true);
    }
    else
        PrintUnicodeLine(formattedMessage, msgLevel == LOG_ERROR);

    if (logInstance->logFile)
    {
        logInstance->logFile->WriteLine(formattedMessage);
        logInstance->logFile->Flush();
    }

    logInstance->inWrite = true;

    using namespace LogMessage;

    VariantMap eventData;
    eventData[MESSAGE] = formattedMessage;
    eventData[LEVEL] = msgLevel;
    logInstance->logMessage.Send(logInstance, eventData);

    logInstance->inWrite = false;
}

void Log::WriteRaw(const String& message, bool error)
{
    Log* logInstance = Subsystem<Log>();
    
    // If not in the main thread, store message for later processing
    if (!Thread::IsMainThread())
    {
        if (logInstance)
        {
            MutexLock lock(logInstance->logMutex);
            logInstance->threadMessages.Push(StoredLogMessage(message, LOG_RAW, error));
        }
        
        return;
    }
    
    // Prevent recursion during log event
    if (!logInstance || logInstance->inWrite)
        return;

    logInstance->lastMessage = message;

    if (logInstance->quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (error)
            PrintUnicode(message, true);
    }
    else
        PrintUnicode(message, error);

    if (logInstance->logFile)
    {
        logInstance->logFile->Write(message.CString(), message.Length());
        logInstance->logFile->Flush();
    }

    logInstance->inWrite = true;

    using namespace LogMessage;

    VariantMap eventData;
    eventData[MESSAGE] = message;
    eventData[LEVEL] = error ? LOG_ERROR : LOG_INFO;
    logInstance->logMessage.Send(logInstance, eventData);

    logInstance->inWrite = false;
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

}
