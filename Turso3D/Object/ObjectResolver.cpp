// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../IO/ObjectRef.h"
#include "ObjectResolver.h"
#include "Serializable.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

void ObjectResolver::StoreObject(unsigned oldId, Serializable* object)
{
    if (object)
        objects[oldId] = object;
}

void ObjectResolver::StoreObjectRef(Serializable* object, Attribute* attr, const ObjectRef& value)
{
    if (object && attr && attr->Type() == ATTR_OBJECTREF)
        objectRefs.Push(StoredObjectRef(object, attr, value.id));
}

void ObjectResolver::Resolve()
{
    for (PODVector<StoredObjectRef>::Iterator it = objectRefs.Begin(); it != objectRefs.End(); ++it)
    {
        HashMap<unsigned, Serializable*>::ConstIterator refIt = objects.Find(it->oldId);
        // See if we can find the referred to object
        if (refIt != objects.End())
        {
            AttributeImpl<ObjectRef>* typedAttr = static_cast<AttributeImpl<ObjectRef>*>(it->attr);
            typedAttr->SetValue(it->object, ObjectRef(refIt->second->Id()));
        }
        else
            LOGWARNING("Could not resolve object reference " + String(it->oldId));
    }
}

}
