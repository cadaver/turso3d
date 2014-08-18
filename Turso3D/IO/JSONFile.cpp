// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/AutoPtr.h"
#include "Deserializer.h"
#include "Serializer.h"
#include "JSONFile.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

JSONFile::JSONFile()
{
}

JSONFile::~JSONFile()
{
}

bool JSONFile::Read(Deserializer& source)
{
    size_t dataSize = source.Size() - source.Position();
    AutoArrayPtr<char> buffer(new char[dataSize]);
    if (source.Read(buffer.Get(), dataSize) != dataSize)
        return false;
    
    const char* pos = buffer.Get();
    const char* end = pos + dataSize;
    
    // Remove any previous content
    root.SetNull();
    /// \todo If fails, log the line number on which the error occurred
    return root.Parse(pos, end);
}

bool JSONFile::Write(Serializer& dest, int spacing) const
{
    String buffer;
    root.ToString(buffer, spacing);
    return dest.Write(buffer.Begin().ptr, buffer.Length()) == buffer.Length();
}

}
