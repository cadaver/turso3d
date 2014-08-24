// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "Resource.h"

namespace Turso3D
{

class Deserializer;
class Serializer;

/// JSON document. Contains a root JSON value and can be read/written to file as text.
class JSONFile : public Resource
{
    OBJECT(JSONFile);

public:
    /// Load from a stream as text. Return true on success. Will contain partial data on failure.
    virtual bool Load(Deserializer& source);
    /// Save to a stream as text. Return true on success.
    virtual bool Save(Serializer& dest) const;
    
    /// Register object factory.
    static void RegisterObject();

    /// Return the root value.
    JSONValue& Root() { return root; }
    /// Return the const root value.
    const JSONValue& Root() const { return root; }

private:
    /// Root value.
    JSONValue root;
};

}
