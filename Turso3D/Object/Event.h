// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Base/AutoPtr.h"
#include "../IO/Variant.h"

namespace Turso3D
{

class Event;

/// Internal helper class for invoking event handler functions.
class TURSO3D_API EventHandler
{
public:
    /// Construct with receiver object pointer.
    EventHandler(WeakRefCounted* receiver_) :
        receiver(receiver_)
    {
        assert(receiver);
    }

    /// Destruct.
    virtual ~EventHandler() {}

    /// Invoke the handler function. Implemented by subclasses.
    virtual void Invoke(Event& event, VariantMap& eventData) = 0;

    /// Return the receiver object.
    WeakRefCounted* Receiver() const { return receiver.Get(); }

protected:
    /// Receiver object.
    WeakPtr<WeakRefCounted> receiver;
};

/// Template implementation of the event handler invoke helper, stores a function pointer of specific class.
template <class T> class EventHandlerImpl : public EventHandler
{
public:
    typedef void (T::*HandlerFunctionPtr)(Event&, VariantMap&);

    /// Construct with receiver and function pointers.
    EventHandlerImpl(T* receiver_, HandlerFunctionPtr function_) :
        EventHandler(receiver_),
        function(function_)
    {
        assert(function);
    }

    /// Invoke the handler function.
    virtual void Invoke(Event& event, VariantMap& eventData)
    {
        T* receiver_ = static_cast<T*>(receiver.Get());
        (receiver_->*function)(event, eventData);
    }

private:
    /// Pointer to the event handler function.
    HandlerFunctionPtr function;
};

/// An event to which objects can subscribe by specifying an event handler function to be called.
class TURSO3D_API Event
{
public:
    /// Construct without name.
    Event();
    /// Construct with name.
    Event(const char* name);
    /// Destruct.
    ~Event();
    
    /// Send the event without parameters.
    void Send(WeakRefCounted* sender);
    /// Send the event with parameter data.
    void Send(WeakRefCounted* sender, VariantMap& eventData);
    /// Subscribe to the event. The event takes ownership of the handler data. If there is already handler data for the same receiver, it is overwritten.
    void Subscribe(EventHandler* handler);
    /// Unsubscribe from the event.
    void Unsubscribe(WeakRefCounted* receiver);
    
    /// Return the event name.
    const char* Name() const { return name; }
    
    /// Return whether has at least one valid receiver.
    bool HasReceivers() const;
    /// Return whether has a specific receiver.
    bool HasReceiver(const WeakRefCounted* receiver) const;
    /// Return current sender.
    WeakRefCounted* Sender() const { return currentSender; }
    
private:
    /// Prevent copy construction.
    Event(const Event& rhs);
    /// Prevent assignment.
    Event& operator = (const Event& rhs);
    
    /// Event handlers.
    Vector<AutoPtr<EventHandler> > handlers;
    /// Current sender.
    WeakPtr<WeakRefCounted> currentSender;
    /// Event name.
    const char* name;
};

#define HANDLER(className, function) (new Turso3D::EventHandlerImpl<className>(this, &className::function))
#define HANDLER_THISPTR(thisPtr, className, function) (new Turso3D::EventHandlerImpl<className>(thisPtr, &className::function))
#define EVENTPARAM(eventName, paramName) namespace eventName { static const Turso3D::StringHash paramName(#paramName); }

}
