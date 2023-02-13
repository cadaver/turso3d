// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Thread/ThreadUtils.h"
#include "Event.h"

EventHandler::EventHandler(RefCounted* receiver_) :
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

void Event::Send(RefCounted* sender)
{
#ifdef _DEBUG
    if (!IsMainThread())
    {
        LOGERROR("Attempted to send an event from outside the main thread");
        return;
    }
#endif

    // Retain a weak pointer to the sender on the stack for safety, in case it is destroyed
    // as a result of event handling, in which case the current event may also be destroyed
    WeakPtr<RefCounted> safeCurrentSender = sender;
    currentSender = sender;
    
    for (auto it = handlers.begin(); it != handlers.end();)
    {
        EventHandler* handler = *it;
        bool remove = true;
        
        if (handler)
        {
            RefCounted* receiver = handler->Receiver();
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
            it = handlers.erase(it);
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
    for (auto it = handlers.begin(); it != handlers.end(); ++it)
    {
        EventHandler* existing = *it;
        if (existing && existing->Receiver() == handler->Receiver())
        {
            *it = handler;
            return;
        }
    }
    
    handlers.push_back(handler);
}

void Event::Unsubscribe(RefCounted* receiver)
{
    for (auto it = handlers.begin(); it != handlers.end(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver() == receiver)
        {
            // If event sending is going on, only clear the pointer but do not remove the element from the handler vector
            // to not confuse the event sending iteration; the element will eventually be cleared by the next SendEvent().
            if (currentSender)
                *it = nullptr;
            else
                handlers.erase(it);
            return;
        }
    }
}

bool Event::HasReceivers() const
{
    for (auto it = handlers.begin(); it != handlers.end(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver())
            return true;
    }
    
    return false;
}

bool Event::HasReceiver(const RefCounted* receiver) const
{
    for (auto it = handlers.begin(); it != handlers.end(); ++it)
    {
        EventHandler* handler = *it;
        if (handler && handler->Receiver() == receiver)
            return true;
    }
    
    return false;
}
