// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "AutoPtr.h"
#include "Ptr.h"

#include <vector>

class Event;

/// Internal helper class for invoking event handler functions.
class EventHandler
{
public:
    /// Construct with receiver object pointer.
    EventHandler(RefCounted* receiver);
    /// Destruct.
    virtual ~EventHandler();

    /// Invoke the handler function. Implemented by subclasses.
    virtual void Invoke(Event& event) = 0;

    /// Return the receiver object.
    RefCounted* Receiver() const { return receiver; }

protected:
    /// Receiver object.
    WeakPtr<RefCounted> receiver;
};

/// Template implementation of the event handler invoke helper, stores a function pointer of specific class.
template <class T, class U> class EventHandlerImpl : public EventHandler
{
public:
    typedef void (T::*HandlerFunctionPtr)(U&);

    /// Construct with receiver and function pointers.
    EventHandlerImpl(RefCounted* receiver_, HandlerFunctionPtr function_) :
        EventHandler(receiver_),
        function(function_)
    {
        assert(function);
    }

    /// Invoke the handler function.
    void Invoke(Event& event) override
    {
        T* typedReceiver = static_cast<T*>(receiver);
        U& typedEvent = static_cast<U&>(event);
        (typedReceiver->*function)(typedEvent);
    }

private:
    /// Pointer to the event handler function.
    HandlerFunctionPtr function;
};

/// Notification and data passing mechanism, to which objects can subscribe by specifying a handler function. Subclass to include event-specific data.
class Event
{
public:
    /// Construct.
    Event();
    /// Destruct.
    virtual ~Event();
    
    /// Send the event.
    void Send(RefCounted* sender);
    /// Subscribe to the event. The event takes ownership of the handler data. If there is already handler data for the same receiver, it is overwritten.
    void Subscribe(EventHandler* handler);
    /// Unsubscribe from the event.
    void Unsubscribe(RefCounted* receiver);

    /// Return whether has at least one valid receiver.
    bool HasReceivers() const;
    /// Return whether has a specific receiver.
    bool HasReceiver(const RefCounted* receiver) const;
    /// Return current sender.
    RefCounted* Sender() const { return currentSender; }
    
private:
    /// Prevent copy construction.
    Event(const Event& rhs);
    /// Prevent assignment.
    Event& operator = (const Event& rhs);
    
    /// Event handlers.
    std::vector<AutoPtr<EventHandler> > handlers;
    /// Current sender.
    WeakPtr<RefCounted> currentSender;
};
