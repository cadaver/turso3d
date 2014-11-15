// For conditions of distribution and use, see copyright notice in License.txt

#include "../Thread/Thread.h"
#include "Object.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

HashMap<StringHash, Object*> Object::subsystems;
HashMap<StringHash, AutoPtr<ObjectFactory> > Object::factories;

ObjectFactory::~ObjectFactory()
{
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

bool Object::IsSubscribedToEvent(const Event& event) const
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
    
    auto it = subsystems.Find(subsystem->Type());
    if (it != subsystems.End() && it->second == subsystem)
        subsystems.Erase(it);
}

void Object::RemoveSubsystem(StringHash type)
{
    subsystems.Erase(type);
}

Object* Object::Subsystem(StringHash type)
{
    auto it = subsystems.Find(type);
    return it != subsystems.End() ? it->second : nullptr;
}

void Object::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;
    
    factories[factory->Type()] = factory;
}

Object* Object::Create(StringHash type)
{
    auto it = factories.Find(type);
    return it != factories.End() ? it->second->Create() : nullptr;
}

const String& Object::TypeNameFromType(StringHash type)
{
    auto it = factories.Find(type);
    return it != factories.End() ? it->second->TypeName() : String::EMPTY;
}

}
