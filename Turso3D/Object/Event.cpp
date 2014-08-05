// For conditions of distribution and use, see copyright notice in License.txt

#include "Event.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static VariantMap emptyEventData;

Event::Event()
{
}

Event::~Event()
{
}

void Event::Send(WeakRefCounted* sender)
{
    // Clear the "empty data" map in case it has been mistakenly filled with data by eg. handler functions
    emptyEventData.Clear();
    Send(sender, emptyEventData);
}

void Event::Send(WeakRefCounted* sender, VariantMap& eventData)
{
    // Retain a weak pointer to the sender on the stack for safety, in case it is destroyed
    // as a result of event handling, in which case the current event may also be destroyed
    WeakPtr<WeakRefCounted> safeCurrentSender = sender;
    currentSender = sender;
    
    for (Vector<AutoPtr<EventHandler> >::Iterator i = handlers.Begin(); i != handlers.End();)
    {
        EventHandler* handler = *i;
        bool remove = true;
        
        if (handler)
        {
            WeakRefCounted* receiver = handler->Receiver();
            if (receiver)
            {
                remove = false;
                handler->Invoke(*this, eventData);
                // If the sender has been destroyed, abort processing immediately
                if (safeCurrentSender.IsExpired())
                    return;
            }
        }
        
        if (remove)
            i = handlers.Erase(i);
        else
            ++i;
    }
    
    currentSender.Reset();
}

void Event::Subscribe(EventHandler* handler)
{
    if (!handler)
        return;
    
    // Check if the same receiver already exists; in that case replace the handler data
    for (Vector<AutoPtr<EventHandler> >::Iterator i = handlers.Begin(); i != handlers.End(); ++i)
    {
        EventHandler* existing = *i;
        if (existing && existing->Receiver() == handler->Receiver())
        {
            *i = handler;
            return;
        }
    }
    
    handlers.Push(handler);
}

void Event::Unsubscribe(WeakRefCounted* receiver)
{
    for (Vector<AutoPtr<EventHandler> >::Iterator i = handlers.Begin(); i != handlers.End(); ++i)
    {
        EventHandler* handler = *i;
        if (handler && handler->Receiver() == receiver)
        {
            // If event sending is going on, only clear the pointer but do not remove the element from the handler vector
            // to not confuse the event sending iteration; the element will eventually be cleared by the next SendEvent().
            if (currentSender)
                *i = (EventHandler*)0;
            else
                handlers.Erase(i);
            return;
        }
    }
}

bool Event::HasReceivers() const
{
    for (Vector<AutoPtr<EventHandler> >::ConstIterator i = handlers.Begin(); i != handlers.End(); ++i)
    {
        EventHandler* handler = *i;
        if (handler && handler->Receiver())
            return true;
    }
    
    return false;
}

bool Event::HasReceiver(const WeakRefCounted* receiver) const
{
    for (Vector<AutoPtr<EventHandler> >::ConstIterator i = handlers.Begin(); i != handlers.End(); ++i)
    {
        EventHandler* handler = *i;
        if (handler && handler->Receiver() == receiver)
            return true;
    }
    
    return false;
}

}
