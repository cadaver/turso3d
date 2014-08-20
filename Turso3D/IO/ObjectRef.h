// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Turso3DConfig.h"

namespace Turso3D
{

/// Reference to an object with id for serialization.
struct TURSO3D_API ObjectRef
{
    /// Object id.
    unsigned id;

    /// Construct with no reference.
    ObjectRef() :
        id(0)
    {
    }

    // Copy-construct.
    ObjectRef(const ObjectRef& ref) :
        id(ref.id)
    {
    }

    /// Construct with object id.
    ObjectRef(unsigned id_) :
        id(id_)
    {
    }

    /// Test for equality with another reference.
    bool operator == (const ObjectRef& rhs) const { return id == rhs.id; }
    /// Test for inequality with another reference.
    bool operator != (const ObjectRef& rhs) const { return !(*this == rhs); }
};

}
