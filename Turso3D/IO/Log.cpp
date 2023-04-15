// For conditions of distribution and use, see copyright notice in License.txt

#include "../Time/TimeUtils.h"
#include "../Thread/ThreadUtils.h"
#include "File.h"
#include "Log.h"

#include <cstdio>
#include <ctime>

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

void Log::Open(const std::string& fileName)
{
    if (fileName.empty())
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
    std::lock_guard<std::mutex> lock(logMutex);

    // Process messages accumulated from other threads (if any)
    while (!threadMessages.empty())
    {
        const StoredLogMessage& stored = threadMessages.front();

        if (stored.level != LOG_RAW)
            Write(stored.level, stored.message);
        else
            WriteRaw(stored.message, stored.error);

        threadMessages.pop_front();
    }
}

void Log::Write(int msgLevel, const std::string& message)
{
    assert(msgLevel >= LOG_DEBUG && msgLevel < LOG_NONE);
    
    Log* instance = Subsystem<Log>();
    if (!instance)
    {
        std::string formattedMessage = logLevelPrefixes[msgLevel];
        formattedMessage += ": " + message;
        fprintf(msgLevel == LOG_ERROR ? stderr : stdout, "%s\n", formattedMessage.c_str());
        return;
    }

    // If not in the main thread, store message for later processing
    if (!IsMainThread())
    {
        std::lock_guard<std::mutex> lock(instance->logMutex);
        instance->threadMessages.push_back(StoredLogMessage(message, msgLevel, false));
        return;
    }

    // Do not log if message level excluded or if currently sending a log event
    if (instance->level > msgLevel || instance->inWrite)
        return;

    std::string formattedMessage = logLevelPrefixes[msgLevel];
    formattedMessage += ": " + message;
    instance->lastMessage = message;

    if (instance->timeStamp)
        formattedMessage = "[" + TimeStamp() + "] " + formattedMessage;
    
    if (instance->quiet)
    {
        // If in quiet mode, still print the error message to the standard error stream
        if (msgLevel == LOG_ERROR)
            fprintf(stderr, "%s\n", formattedMessage.c_str());
    }
    else
        fprintf(msgLevel == LOG_ERROR ? stderr : stdout, "%s\n", formattedMessage.c_str());

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

void Log::WriteRaw(const std::string& message, bool error)
{
    Log* instance = Subsystem<Log>();
    if (!instance)
    {
        fprintf(error ? stderr : stdout, "%s\n", message.c_str());
        return;
    }

    // If not in the main thread, store message for later processing
    if (!IsMainThread())
    {
        std::lock_guard<std::mutex> lock(instance->logMutex);
        instance->threadMessages.push_back(StoredLogMessage(message, LOG_RAW, error));
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
            fprintf(stderr, "%s\n", message.c_str());
    }
    else
        fprintf(error ? stderr : stdout, "%s\n", message.c_str());

    if (instance->logFile)
    {
        instance->logFile->Write(message.c_str(), message.length());
        instance->logFile->Flush();
    }

    instance->inWrite = true;

    LogMessageEvent& event = instance->logMessageEvent;
    event.message = message;
    event.level = error ? LOG_ERROR : LOG_INFO;
    instance->SendEvent(event);

    instance->inWrite = false;
}
