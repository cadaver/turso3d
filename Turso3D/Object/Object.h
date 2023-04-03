// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/StringHash.h"
#include "Allocator.h"
#include "Event.h"

#include <map>
#include <set>

class ObjectFactory;
template <class T> class ObjectFactoryImpl;

/// Base class for objects with type identification and possibility to create through a factory.
class Object : public RefCounted
{
public:
    /// Release a strong reference. Destroy the object when the last strong reference is gone. If a factory exists for the object's type, it will be destroyed through it.
    void ReleaseRef() override;

    /// Return hash of the type name.
    virtual StringHash Type() const = 0;
    /// Return type name.
    virtual const std::string& TypeName() const = 0;

    /// Subscribe to an event.
    void SubscribeToEvent(Event& event, EventHandler* handler);
    /// Unsubscribe from an event.
    void UnsubscribeFromEvent(Event& event);
    /// Send an event.
    void SendEvent(Event& event);
    
    /// Subscribe to an event, template version.
    template <class T, class U> void SubscribeToEvent(U& event, void (T::*handlerFunction)(U&))
    {
        SubscribeToEvent(event, new EventHandlerImpl<T, U>(this, handlerFunction)); 
    }

    /// Return whether is subscribed to an event.
    bool SubscribedToEvent(const Event& event) const;
    
    /// Register an object as a subsystem that can be accessed globally. Note that the subsystems container does not own the objects.
    static void RegisterSubsystem(Object* subsystem);
    /// Remove a subsystem by object pointer.
    static void RemoveSubsystem(Object* subsystem);
    /// Remove a subsystem by type.
    static void RemoveSubsystem(StringHash type);
    /// Return a subsystem by type, or null if not registered.
    static Object* Subsystem(StringHash type);
    /// Register an object factory.
    static void RegisterFactory(ObjectFactory* factory);
    /// Register a derived object type.
    static void RegisterDerivedType(StringHash derived, StringHash base);
    /// Create and return an object through a factory. The caller is assumed to take ownership of the object. Return null if no factory registered. 
    static Object* Create(StringHash type);
    /// Destroy an object through a factory if exists, or if not, then by deleting.
    static void Destroy(Object* object);
    /// Return a type name from hash, or empty if not known. Requires a registered object factory.
    static const std::string& TypeNameFromType(StringHash type);
    /// Return whether type is derived from another type.
    static bool DerivedFrom(StringHash derived, StringHash base);
    /// Return a subsystem, template version.
    template <class T> static T* Subsystem() { return static_cast<T*>(Subsystem(T::TypeStatic())); }
    /// Register an object factory, template version.
    template <class T> static void RegisterFactory(size_t initialCapacity = DEFAULT_ALLOCATOR_INITIAL_CAPACITY) { RegisterFactory(new ObjectFactoryImpl<T>(initialCapacity)); }
    /// Register a derived type, template version.
    template <class T, class U> static void RegisterDerivedType() { RegisterDerivedType(T::TypeStatic(), U::TypeStatic()); }
    /// Create and return an object through a factory, template version.
    template <class T> static T* Create() { return static_cast<T*>(Create(T::TypeStatic())); }
    
private:
    /// Registered subsystems.
    static std::map<StringHash, Object*> subsystems;
    /// Registered object factories.
    static std::map<StringHash, AutoPtr<ObjectFactory> > factories;
    /// Registered derived types.
    static std::set<std::pair<StringHash, StringHash> > derivedTypes;
    /// Registered immediate base types.
    static std::map<StringHash, StringHash> baseTypes;
};

/// Base class for object factories.
class ObjectFactory
{
public:
    /// Destruct.
    virtual ~ObjectFactory();
    
    /// Create and return an object.
    virtual Object* Create() = 0;
    /// Destroy an object created through the factory.
    virtual void Destroy(Object* object) = 0;

    /// Return type name hash of the objects created by this factory.
    StringHash Type() const { return type; }
    /// Return type name of the objects created by this factory.
    const std::string& TypeName() const { return typeName; }

protected:
    /// %Object type name hash.
    StringHash type;
    /// %Object type name.
    std::string typeName;
};

/// Template implementation of the object factory.
template <class T> class ObjectFactoryImpl : public ObjectFactory
{
public:
    /// Construct.
    ObjectFactoryImpl(size_t initialCapacity = DEFAULT_ALLOCATOR_INITIAL_CAPACITY) :
        allocator(initialCapacity)
    {
        type = T::TypeStatic();
        typeName = T::TypeNameStatic();
    }

    /// Create and return an object of the specific type.
    Object* Create() override { return allocator.Allocate(); }
    /// Destroy an object created through the factory.
    void Destroy(Object* object) override { return allocator.Free(static_cast<T*>(object)); }

private:
    /// Allocator for the objects.
    Allocator<T> allocator;
};

#define OBJECT(typeName) \
    private: \
        static const StringHash typeStatic; \
        static const std::string typeNameStatic; \
    public: \
        StringHash Type() const override { return TypeStatic(); } \
        const std::string& TypeName() const override { return TypeNameStatic(); } \
        static StringHash TypeStatic() { static const StringHash type(#typeName); return type; } \
        static const std::string& TypeNameStatic() { static const std::string type(#typeName); return type; } \

