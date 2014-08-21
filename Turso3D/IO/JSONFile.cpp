// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/AutoPtr.h"
#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "Deserializer.h"
#include "File.h"
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

bool JSONFile::Load(Deserializer& source)
{
    PROFILE(LoadJSONFile);
    
    size_t dataSize = source.Size() - source.Position();
    AutoArrayPtr<char> buffer(new char[dataSize]);
    if (source.Read(buffer.Get(), dataSize) != dataSize)
        return false;
    
    const char* pos = buffer.Get();
    const char* end = pos + dataSize;
    
    // Remove any previous content
    root.SetNull();
    /// \todo If fails, log the line number on which the error occurred
    bool success = root.Parse(pos, end);
    if (!success)
    {
        File* sourceFile = dynamic_cast<File*>(&source);
        if (sourceFile)
            LOGERROR("Parsing JSON from " + sourceFile->Name() + " failed; data may be partial");
        else
            LOGERROR("Parsing JSON failed; data may be partial");
    }
    
    return success;
}

bool JSONFile::Save(Serializer& dest, int spacing) const
{
    PROFILE(SaveJSONFile);
    
    String buffer;
    root.ToString(buffer, spacing);
    return dest.Write(buffer.Begin().ptr, buffer.Length()) == buffer.Length();
}

}
