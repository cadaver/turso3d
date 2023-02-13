// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Object.h"
#include "StringUtils.h"

#include <list>
#include <mutex>

#define USE_LOG

/// Fictional message level to indicate a stored raw message.
static const int LOG_RAW = -1;
/// Debug message level. By default only shown in debug mode.
static const int LOG_DEBUG = 0;
/// Informative message level.
static const int LOG_INFO = 1;
/// Warning message level.
static const int LOG_WARNING = 2;
/// Error message level.
static const int LOG_ERROR = 3;
/// Disable all log messages.
static const int LOG_NONE = 4;

class File;

/// Stored log message from another thread.
struct StoredLogMessage
{
    /// Construct undefined.
    StoredLogMessage()
    {
    }
    
    /// Construct with parameters.
    StoredLogMessage(const std::string& message_, int level_, bool error_) :
        message(message_),
        level(level_),
        error(error_)
    {
    }
    
    /// Message text.
    std::string message;
    /// Message level. -1 for raw messages.
    int level;
    /// Error flag for raw messages.
    bool error;
};

/// %Log message event.
class LogMessageEvent : public Event
{
public:
    /// Message.
    std::string message;
    /// Message level.
    int level;
};

/// Logging subsystem.
class Log : public Object
{
    OBJECT(Log);

public:
    /// Construct and register subsystem.
    Log();
    /// Destruct. Close the log file if open.
    ~Log();

    /// Open the log file.
    void Open(const std::string& fileName);
    /// Close the log file.
    void Close();
    /// Set logging level.
    void SetLevel(int newLevel);
    /// Set whether to timestamp log messages.
    void SetTimeStamp(bool enable);
    /// Set quiet mode, ie. only output error messages to the standard error stream.
    void SetQuiet(bool enable);
    /// Process threaded log messages at the end of a frame.
    void EndFrame();

    /// Return logging level.
    int Level() const { return level; }
    /// Return whether log messages are timestamped.
    bool HasTimeStamp() const { return timeStamp; }
    /// Return last log message.
    const std::string& LastMessage() const { return lastMessage; }

    /// Write to the log. If logging level is higher than the level of the message, the message is ignored.
    static void Write(int msgLevel, const std::string& message);
    /// Write raw output to the log.
    static void WriteRaw(const std::string& message, bool error = false);

    /// %Log message event.
    LogMessageEvent logMessageEvent;

private:
    /// Mutex for threaded operation.
    std::mutex logMutex;
    /// %Log messages from other threads.
    std::list<StoredLogMessage> threadMessages;
    /// %Log file.
    AutoPtr<File> logFile;
    /// Last log message.
    std::string lastMessage;
    /// Logging level.
    int level;
    /// Use timestamps flag.
    bool timeStamp;
    /// In write flag to prevent recursion.
    bool inWrite;
    /// Quite mode flag.
    bool quiet;
};

#ifdef USE_LOG

#ifdef _DEBUG
#define LOGDEBUG(message) Log::Write(LOG_DEBUG, message)
#define LOGDEBUGF(format, ...) Log::Write(LOG_DEBUG, FormatString(format, ##__VA_ARGS__))
#else
#define LOGDEBUG(message) 
#define LOGDEBUGF(format, ...)
#endif

#define LOGINFO(message) Log::Write(LOG_INFO, message)
#define LOGWARNING(message) Log::Write(LOG_WARNING, message)
#define LOGERROR(message) Log::Write(LOG_ERROR, message)
#define LOGRAW(message) Log::WriteRaw(message)
#define LOGINFOF(format, ...) Log::Write(LOG_INFO, FormatString(format, ##__VA_ARGS__))
#define LOGWARNINGF(format, ...) Log::Write(LOG_WARNING, FormatString(format, ##__VA_ARGS__))
#define LOGERRORF(format, ...) Log::Write(LOG_ERROR, FormatString(format, ##__VA_ARGS__))
#define LOGRAWF(format, ...) Log::WriteRaw(FormatString(format, ##__VA_ARGS__))

#else

#define LOGDEBUG(message)
#define LOGINFO(message)
#define LOGWARNING(message)
#define LOGERROR(message)
#define LOGRAW(message)
#define LOGDEBUGF(format, ...)
#define LOGINFOF(format, ...)
#define LOGWARNINGF(format, ...)
#define LOGERRORF(format, ...)
#define LOGRAWF(format, ...)

#endif
