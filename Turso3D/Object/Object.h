// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "Event.h"

namespace Turso3D
{

/// Base class for objects with type identification and possibility to create through a factory.
class TURSO3D_API Object : public WeakRefCounted
{
public:
    /// Return hash of the type name.
    virtual StringHash Type() const = 0;
    /// Return type name.
    virtual const String& TypeName() const = 0;

    /// Subscribe to an event.
    void SubscribeToEvent(Event& event, EventHandler* handler);
    /// Unsubscribe from an event.
    void UnsubscribeFromEvent(Event& event);
    /// Send an event without parameters.
    void SendEvent(Event& event);
    /// Send an event with parameter data.
    void SendEvent(Event& event, VariantMap& eventData);

    /// Return whether is subscribed to an event.
    bool IsSubscribedToEvent(const Event& event) const;
};

/// Base class for object factories.
class ObjectFactory
{
public:
    /// Destruct.
    virtual ~ObjectFactory() {}
    
    /// Create and return an object.
    virtual Object* CreateObject() = 0;

    /// Return type name hash of the objects created by this factory.
    StringHash Type() const { return type; }
    /// Return type name of the objects created by this factory.
    const String& TypeName() const { return typeName; }

protected:
    /// Object type name hash.
    StringHash type;
    /// Object type name.
    String typeName;
};

/// Template implementation of the object factory.
template <class T> class ObjectFactoryImpl : public ObjectFactory
{
public:
    /// Construct.
    ObjectFactoryImpl()
    {
        type = T::TypeStatic();
        typeName = T::TypeNameStatic();
    }

    /// Create and return an object of the specific type.
    virtual Object* CreateObject() { return new T(); }
};

/// Register an object as a subsystem that can be accessed globally. Note that the subsystems container does not own the objects.
TURSO3D_API void RegisterSubsystem(Object* subsystem);
/// Remove a subsystem by object pointer.
TURSO3D_API void RemoveSubsystem(Object* subsystem);
/// Remove a subsystem by type.
TURSO3D_API void RemoveSubsystem(StringHash type);
/// Return a subsystem by type, or null if not registered.
TURSO3D_API Object* Subsystem(StringHash type);
/// Register an object factory.
TURSO3D_API void RegisterFactory(ObjectFactory* factory);
/// Create and return an object through a factory. Return null if no factory registered.
TURSO3D_API Object* CreateObject(StringHash type);
/// Return a type name from hash, or empty if not known. Requires a registered object factory.
TURSO3D_API const String& TypeNameFromType(StringHash type);
/// Tempate version of returning a subsystem.
template <class T> T* Subsystem() { return static_cast<T*>(Subsystem(T::TypeStatic())); }
/// Template version of registering an object factory.
template <class T> void RegisterFactory() { RegisterFactory(new ObjectFactoryImpl<T>()); }
/// Template version of creating and returning an object through a factory.
template <class T> T* CreateObject() { return static_cast<T*>(CreateObject(T::TypeStatic())); }

}

#define OBJECT(typeName) \
    private: \
        static const Turso3D::StringHash typeStatic; \
        static const Turso3D::String typeNameStatic; \
    public: \
        virtual Turso3D::StringHash Type() const { return TypeStatic(); } \
        virtual const Turso3D::String& TypeName() const { return TypeNameStatic(); } \
        static Turso3D::StringHash TypeStatic() { static const Turso3D::StringHash type(#typeName); return type; } \
        static const Turso3D::String& TypeNameStatic() { static const Turso3D::String type(#typeName); return type; } \

