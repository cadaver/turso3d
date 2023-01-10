// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../IO/ObjectRef.h"
#include "../IO/StringUtils.h"
#include "ObjectResolver.h"
#include "Serializable.h"

void ObjectResolver::StoreObject(unsigned oldId, Serializable* object)
{
    if (object)
        objects[oldId] = object;
}

void ObjectResolver::StoreObjectRef(Serializable* object, Attribute* attr, const ObjectRef& value)
{
    if (object && attr && attr->Type() == ATTR_OBJECTREF)
        objectRefs.push_back(StoredObjectRef(object, attr, value.id));
}

void ObjectResolver::Resolve()
{
    for (auto it = objectRefs.begin(); it != objectRefs.end(); ++it)
    {
        auto refIt = objects.find(it->oldId);
        // See if we can find the referred object
        if (refIt != objects.end())
        {
            AttributeImpl<ObjectRef>* typedAttr = static_cast<AttributeImpl<ObjectRef>*>(it->attr);
            typedAttr->SetValue(it->object, ObjectRef(refIt->second->Id()));
        }
        else
            LOGWARNING("Could not resolve object reference " + ToString(it->oldId));
    }
}
