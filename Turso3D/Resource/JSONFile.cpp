// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../IO/Stream.h"
#include "JSONFile.h"

#include <tracy/Tracy.hpp>

void JSONFile::RegisterObject()
{
    RegisterFactory<JSONFile>();
}

bool JSONFile::BeginLoad(Stream& source)
{
    ZoneScoped;
    
    size_t dataSize = source.Size() - source.Position();
    AutoArrayPtr<char> buffer(new char[dataSize]);
    if (source.Read(buffer, dataSize) != dataSize)
        return false;
    
    const char* pos = buffer;
    const char* end = pos + dataSize;
    
    // Remove any previous content
    root.SetNull();
    /// \todo If fails, log the line number on which the error occurred
    bool success = root.Parse(pos, end);
    if (!success)
    {
        LOGERROR("Parsing JSON from " + source.Name() + " failed; data may be partial");
    }

    return success;
}

bool JSONFile::Save(Stream& dest)
{
    ZoneScoped;
    
    std::string buffer;
    root.ToString(buffer);
    if (buffer.length())
        return dest.Write(&buffer[0], buffer.length()) == buffer.length();
    else
        return true;
}
