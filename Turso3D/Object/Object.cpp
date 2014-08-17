// For conditions of distribution and use, see copyright notice in License.txt

#include "../Thread/Thread.h"
#include "Object.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

HashMap<StringHash, Object*> Object::subsystems;
HashMap<StringHash, AutoPtr<ObjectFactory> > Object::factories;

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
    
    HashMap<StringHash, Object*>::Iterator i = subsystems.Find(subsystem->Type());
    if (i != subsystems.End() && i->second == subsystem)
        subsystems.Erase(i);
}

void Object::RemoveSubsystem(StringHash type)
{
    subsystems.Erase(type);
}

Object* Object::Subsystem(StringHash type)
{
    HashMap<StringHash, Object*>::Iterator i = subsystems.Find(type);
    return i != subsystems.End() ? i->second : (Object*)0;
}

void Object::RegisterFactory(ObjectFactory* factory)
{
    if (!factory)
        return;
    
    factories[factory->Type()] = factory;
}

Object* Object::Create(StringHash type)
{
    HashMap<StringHash, AutoPtr<ObjectFactory> >::Iterator i = factories.Find(type);
    return i != factories.End() ? i->second->Create() : (Object*)0;
}

const String& Object::TypeNameFromType(StringHash type)
{
    HashMap<StringHash, AutoPtr<ObjectFactory> >::Iterator i = factories.Find(type);
    return i != factories.End() ? i->second->TypeName() : String::EMPTY;
}

}
