// For conditions of distribution and use, see copyright notice in License.txt

#include "File.h"
#include "FileSystem.h"

#include <cstdio>

static const char* openModes[] =
{
    "rb",
    "wb",
    "r+b",
    "w+b"
};

File::File() :
    mode(FILE_READ),
    handle(nullptr),
    readSyncNeeded(false),
    writeSyncNeeded(false)
{
}

File::File(const std::string& fileName, FileMode mode) :
    mode(FILE_READ),
    handle(nullptr),
    readSyncNeeded(false),
    writeSyncNeeded(false)
{
    Open(fileName, mode);
}

File::~File()
{
    Close();
}

bool File::Open(const std::string& fileName, FileMode fileMode)
{
    Close();

    if (fileName.empty())
        return false;
    
    handle = fopen(NativePath(fileName).c_str(), openModes[fileMode]);

    // If file did not exist in readwrite mode, retry with write-update mode
    if (mode == FILE_READWRITE && !handle)
    {
        handle = fopen(NativePath(fileName).c_str(), openModes[fileMode + 1]);
    }
    
    if (!handle)
        return false;

    name = fileName;
    mode = fileMode;
    position = 0;
    readSyncNeeded = false;
    writeSyncNeeded = false;

    fseek((FILE*)handle, 0, SEEK_END);
    size = ftell((FILE*)handle);
    fseek((FILE*)handle, 0, SEEK_SET);
    return true;
}

size_t File::Read(void* dest, size_t numBytes)
{
    if (!handle || mode == FILE_WRITE)
        return 0;

    if (numBytes + position > size)
        numBytes = size - position;
    if (!numBytes)
        return 0;

    // Need to reassign the position due to internal buffering when transitioning from writing to reading
    if (readSyncNeeded)
    {
        fseek((FILE*)handle, (long)position, SEEK_SET);
        readSyncNeeded = false;
    }
    
    size_t ret = fread(dest, numBytes, 1, (FILE*)handle);
    if (ret != 1)
    {
        // If error, return to the position where the read began
        fseek((FILE*)handle, (long)position, SEEK_SET);
        return 0;
    }

    writeSyncNeeded = true;
    position += numBytes;
    return numBytes;
}

size_t File::Seek(size_t newPosition)
{
    if (!handle)
        return 0;
    
    // Allow sparse seeks if writing
    if (mode == FILE_READ && newPosition > size)
        newPosition = size;

    fseek((FILE*)handle, (long)newPosition, SEEK_SET);
    position = newPosition;
    readSyncNeeded = false;
    writeSyncNeeded = false;
    return position;
}

size_t File::Write(const void* data, size_t numBytes)
{
    if (!handle || mode == FILE_READ)
        return 0;

    if (!numBytes)
        return 0;

    // Need to reassign the position due to internal buffering when transitioning from reading to writing
    if (writeSyncNeeded)
    {
        fseek((FILE*)handle, (long)position, SEEK_SET);
        writeSyncNeeded = false;
    }
    
    if (fwrite(data, numBytes, 1, (FILE*)handle) != 1)
    {
        // If error, return to the position where the write began
        fseek((FILE*)handle, (long)position, SEEK_SET);
        return 0;
    }

    readSyncNeeded = true;
    position += numBytes;
    if (position > size)
        size = position;

    return size;
}

bool File::IsReadable() const
{
    return handle != 0 && mode != FILE_WRITE;
}

bool File::IsWritable() const
{
    return handle != 0 && mode != FILE_READ;
}

void File::Close()
{
    if (handle)
    {
        fclose((FILE*)handle);
        handle = 0;
        position = 0;
        size = 0;
    }
}

void File::Flush()
{
    if (handle)
        fflush((FILE*)handle);
}

bool File::IsOpen() const
{
    return handle != 0;
}
