// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Thread/Thread.h"
#include "Event.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

EventHandler::EventHandler(WeakRefCounted* receiver_) :
    receiver(receiver_)
{
}

EventHandler::~EventHandler()
{
}

Event::Event()
{
}

Event::~Event()
{
}

void Event::Send(WeakRefCounted* sender)
{
    if (!Thread::IsMainThread())
    {
        LOGERROR("Attempted to send an event from outside the main thread");
        return;
    }

    // Retain a weak pointer to the sender on the stack for safety, in case it is destroyed
    // as a result of event handling, in which case the current event may also be destroyed
    WeakPtr<WeakRefCounted> safeCurrentSender = sender;
    currentSender = sender;
    
    for (Vector<AutoPtr<EventHandler> >::Iterator it = handlers.Begin(); it != handlers.End();)
    {
        EventHandler* handler = *it;
        bool remove = true;
        
        if (handler)
        {
            WeakRefCounted* receiver = handler->Receiver();
            if (receiver)
            {
                remove = false;
                handler->Invoke(*this);
                // If the sender has been destroyed, abort processing immediately
                if (safeCurrentSender.IsExpired())
                    return;
            }
        }
        
        if (remove)
            it = handlers.Erase(it);
        else
            ++it;
    }
    
    currentSender.Reset();
}

void Event::Subscribe(EventHandler* handler)
{
    if (!handler)
        return;
    
    // Check if the same receiver already exists; in that case replace the handler data
    for (Vector<AutoPtr<EventHandler> >::Iterator it = handlers.Begin(); it != handlers.End(); ++it)
    {
        EventHandler* existing = *it;
        if (existing && existing->Receiver() == handler->Receiver())
        {
            *it = handler;
            return;
        }
    }
    
    handlers.Push(handler);
}

void Event::Unsubscribe(WeakRefCounted* receiver)
{
    for (Vector<AutoPtr<EventHandler> >::Iterator it = handlers.Begin(); it != handlers.End(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver() == receiver)
        {
            // If event sending is going on, only clear the pointer but do not remove the element from the handler vector
            // to not confuse the event sending iteration; the element will eventually be cleared by the next SendEvent().
            if (currentSender)
                *it = (EventHandler*)0;
            else
                handlers.Erase(it);
            return;
        }
    }
}

bool Event::HasReceivers() const
{
    for (Vector<AutoPtr<EventHandler> >::ConstIterator it = handlers.Begin(); it != handlers.End(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver())
            return true;
    }
    
    return false;
}

bool Event::HasReceiver(const WeakRefCounted* receiver) const
{
    for (Vector<AutoPtr<EventHandler> >::ConstIterator it = handlers.Begin(); it != handlers.End(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver() == receiver)
            return true;
    }
    
    return false;
}

}
