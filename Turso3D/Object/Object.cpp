// For conditions of distribution and use, see copyright notice in License.txt

#include "Object.h"
#include "../IO/JSONValue.h"

std::map<StringHash, Object*> Object::subsystems;
std::map<StringHash, AutoPtr<ObjectFactory> > Object::factories;
std::set<std::pair<StringHash, StringHash> > Object::derivedTypes;
std::map<StringHash, StringHash> Object::baseTypes;

ObjectFactory::~ObjectFactory()
{
}

void Object::ReleaseRef()
{
    assert(refCount && refCount->refs > 0);
    --(refCount->refs);
    if (refCount->refs == 0)
        Destroy(this);
}

void Object::SubscribeToEvent(Event& event, EventHandler* handler)
{
    event.Subscribe(handler);
}

void Object::UnsubscribeFromEvent(Event& event)
{
    event.Unsubscribe(this);
}

void Object::SendEvent(Event& event)
{
    event.Send(this);
}

bool Object::SubscribedToEvent(const Event& event) const
{
    return event.HasReceiver(this);
}

void Object::RegisterSubsystem(Object* subsystem)
{
    if (!subsystem)
        return;
    
    subsystems[subsystem->Type()] = subsystem;
}

void Object::RemoveSubsystem(Object* subsystem)
{
    if (!subsystem)
        return;
    
    auto it = subsystems.find(subsystem->Type());
    if (it != subsystems.end() && it->second == subsystem)
        subsystems.erase(it);
}

void Object::RemoveSubsystem(StringHash type)
{
    subsystems.erase(type);
}

Object* Object::Subsystem(StringHash type)
{
    auto it = subsystems.find(type);
    return it != subsystems.end() ? it->second : nullptr;
}

void Object::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;
    
    factories[factory->Type()] = factory;
}

Object* Object::Create(StringHash type)
{
    auto it = factories.find(type);
    return it != factories.end() ? it->second->Create() : nullptr;
}

void Object::Destroy(Object* object)
{
    assert(object);

    auto it = factories.find(object->Type());
    if (it != factories.end())
        it->second->Destroy(object);
    else
        delete object;
}

const std::string& Object::TypeNameFromType(StringHash type)
{
    auto it = factories.find(type);
    return it != factories.end() ? it->second->TypeName() : JSONValue::emptyString;
}

bool Object::DerivedFrom(StringHash derived, StringHash base)
{
    return derivedTypes.find(std::make_pair(derived, base)) != derivedTypes.end();
}

void Object::RegisterDerivedType(StringHash derived, StringHash base)
{
    baseTypes[derived] = base;

    derivedTypes.insert(std::make_pair(derived, base));
    
    auto it = baseTypes.find(base);
    // Register the whole chain
    while (it != baseTypes.end())
    {
        base = it->second;
        derivedTypes.insert(std::make_pair(derived, base));
        it = baseTypes.find(base);
    }
}
