// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/WeakPtr.h"
#include "JSONValue.h"

namespace Turso3D
{

class Deserializer;
class Serializer;

/// JSON document. Contains a root JSON value and can be read/written to file as text.
class JSONFile : public WeakRefCounted
{
public:
    /// Construct empty.
    JSONFile();
    /// Destruct.
    virtual ~JSONFile();
    
    /// Read from a stream as text. Return true on success. Will contain partial data on failure.
    bool Read(Deserializer& source);
    /// Write to a stream as text. Return true on success.
    bool Write(Serializer& dest) const;
    
    /// Return the root value.
    JSONValue& Root() { return root; }
    /// Return the const root value.
    const JSONValue& Root() const { return root; }

private:
    /// Root value.
    JSONValue root;
};

}
