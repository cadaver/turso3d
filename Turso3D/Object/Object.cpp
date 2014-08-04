// For conditions of distribution and use, see copyright notice in License.txt

#include "Object.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

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

void Object::SendEvent(Event& event, VariantMap& eventData)
{
    event.Send(this, eventData);
}

bool Object::IsSubscribedToEvent(const Event& event) const
{
    return event.HasReceiver(this);
}

}
