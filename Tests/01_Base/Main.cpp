// For conditions of distribution and use, see copyright notice in License.txt

#include "Turso3D.h"
#include "Debug/DebugNew.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>

using namespace Turso3D;

class Test
{
public:
    Test()
    {
        printf("Test constructed\n");
    }

    ~Test()
    {
        printf("Test destroyed\n");
    }
};

class TestShared: public RefCounted
{
public:
    TestShared()
    {
        printf("TestShared constructed\n");
    }

    ~TestShared()
    {
        printf("TestShared destroyed\n");
    }
};

class TestReferenced : public WeakRefCounted
{
public:
    TestReferenced()
    {
        printf("TestReferenced constructed\n");
    }

    ~TestReferenced()
    {
        printf("TestReferenced destroyed\n");
    }
};

class TestEventSender : public Object
{
    OBJECT(TestEventSender);
    
public:
    void SendTestEvent()
    {
        SendEvent(testEvent);
    }
    
    Event testEvent;
};

class TestEventReceiver : public Object
{
    OBJECT(TestEventReceiver);
    
public:
    void SubscribeToTestEvent(TestEventSender* sender)
    {
        SubscribeToEvent(sender->testEvent, HANDLER(TestEventReceiver, HandleTestEvent));
    }
    
    void HandleTestEvent(Event& event, VariantMap& /* eventData */)
    {
        printf("Receiver %08x got event %08x from %08x\n", (int)this, (int)&event, (int)event.Sender());
    }
};

const size_t NUM_ITEMS = 10000;

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    printf("\nSize of String: %d\n", sizeof(String));
    printf("Size of Vector: %d\n", sizeof(Vector<int>));
    printf("Size of List: %d\n", sizeof(List<int>));
    printf("Size of HashMap: %d\n", sizeof(HashMap<int, int>));
    printf("Size of RefCounted: %d\n", sizeof(RefCounted));
    printf("Size of WeakRefCounted: %d\n", sizeof(WeakRefCounted));
    printf("Size of Variant: %d\n", sizeof(Variant));
    printf("Size of JSONValue: %d\n", sizeof(JSONValue));
    
    {
        printf("\nTesting AutoPtr assignment\n");
        AutoPtr<Test> ptr1(new Test);
        AutoPtr<Test> ptr2;
        ptr2 = ptr1;
    }

    {
        printf("\nTesting AutoPtr copy construction\n");
        AutoPtr<Test> ptr1(new Test);
        AutoPtr<Test> ptr2(ptr1);
    }

    {
        printf("\nTesting AutoPtr detaching\n");
        AutoPtr<Test> ptr1(new Test);
        // We would now have a memory leak if we don't manually delete the object
        Test* object = ptr1.Detach();
        delete object;
    }
    
    {
        printf("\nTesting SharedPtr\n");
        SharedPtr<TestShared> ptr1(new TestShared);
        SharedPtr<TestShared> ptr2(ptr1);
        printf("Number of refs: %d\n", ptr1.Refs());
    }
    
    {
        printf("\nTesting WeakPtr\n");
        TestReferenced* object = new TestReferenced;
        WeakPtr<TestReferenced> ptr1(object);
        WeakPtr<TestReferenced> ptr2(ptr1);
        printf("Number of weak refs: %d expired: %d\n", ptr1.WeakRefs(), ptr1.IsExpired());
        ptr2.Reset();
        delete object;
        printf("Number of weak refs: %d expired: %d\n", ptr1.WeakRefs(), ptr1.IsExpired());
    }

    {
        printf("\nTesting Vector\n");
        Vector<int> vec;
        srand(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            vec.Push(rand());
        int sum = 0;
        for (Vector<int>::ConstIterator i = vec.Begin(); i != vec.End(); ++i)
            sum += *i;
        printf("Size: %d capacity: %d\n", vec.Size(), vec.Capacity());
        printf("Sum of vector items: %d\n", sum);
    }
    
    {
        printf("\nTesting List\n");
        List<int> list;
        srand(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            list.Push(rand());
        int sum = 0;
        for (List<int>::ConstIterator i = list.Begin(); i != list.End(); ++i)
            sum += *i;
        printf("Size: %d\n", list.Size());
        printf("Sum of list items: %d\n", sum);
    }
    
    {
        printf("\nTesting String\n");
        String test;
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            test += "Test";
        String test2;
        test2.AppendWithFormat("Size: %d capacity: %d\n", test.Length(), test.Capacity());
        printf(test2.CString());
        test2 = test2.ToUpper();
        printf(test2.CString());
        test2.Replace("SIZE:", "LENGTH:");
        printf(test2.CString());
    }
    
    {
        printf("\nTesting HashSet\n");
        size_t found = 0;
        unsigned sum = 0;
        HashSet<int> testHashSet;
        srand(0);
        found = 0;
        sum = 0;
        printf("Insert, search and iteration, %d keys\n", NUM_ITEMS);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
        {
            int number = (rand() & 32767);
            testHashSet.Insert(number);
        }
        for (int i = 0; i < 32768; ++i)
        {
            if (testHashSet.Find(i) != testHashSet.End())
                ++found;
        }
        for (HashSet<int>::Iterator i = testHashSet.Begin(); i != testHashSet.End(); ++i)
            sum += *i;
        printf("Set size and sum: %d %d\n", testHashSet.Size(), sum);
        for (unsigned i = 0; i < 32768; ++i)
            testHashSet.Erase(i);
        printf("Set size after erase: %d\n", testHashSet.Size());
    }

    {
        printf("\nTesting AutoPtr inside a vector\n");
        Vector<AutoPtr<Test> > vec;
        printf("Filling vector\n");
        for (size_t i = 0; i < 16; ++i)
            vec.Push(new Test());
        printf("Clearing vector\n");
        vec.Clear();
    }

    {
        printf("\nTesting Variant\n");
        Variant var1 = true;
        Variant var2 = 100;
        Variant var3 = "Test";
        Variant var4 = Vector3::UP;
        printf("Variant 1 type %s Value %d\n", var1.TypeName().CString(), var1.AsBool());
        printf("Variant 2 type %s Value %d\n", var2.TypeName().CString(), var2.AsInt());
        printf("Variant 3 type %s Value %s\n", var3.TypeName().CString(), var3.AsString().CString());
        printf("Variant 4 type %s Value %s\n", var4.TypeName().CString(), var4.AsVector3().ToString().CString());
    }
    
    {
        printf("\nTesting objects & events\n");
        RegisterFactory<TestEventSender>();
        RegisterFactory<TestEventReceiver>();
        
        TestEventSender* sender = CreateObject<TestEventSender>();
        TestEventReceiver* receiver1 = CreateObject<TestEventReceiver>();
        TestEventReceiver* receiver2 = CreateObject<TestEventReceiver>();
        printf("Type of sender is %s\n", sender->TypeName().CString());
        receiver1->SubscribeToTestEvent(sender);
        receiver2->SubscribeToTestEvent(sender);
        sender->SendTestEvent();
        delete receiver2;
        sender->SendTestEvent();
        delete receiver1;
        delete sender;
    }
    
    {
        printf("\nTesting logging and profiling\n");
        Log log;
        Profiler profiler;
        
        profiler.BeginFrame();
        
        {
            PROFILE(OpenLog);
            log.Open("01_Base.log");
        }
        
        {
            PROFILE(WriteMessages);
            LOGDEBUG("Debug message");
            LOGINFO("Info message");
            LOGERROR("Error message");
            LOGINFOF("Formatted message: %d", 100);
        }
        
        profiler.EndFrame();
        
        printf("%s\n", profiler.OutputResults().CString());
    }
    
    {
        printf("Testing JSONValue\n");
        JSONValue org;
        org["name"] = "S.C.E.P.T.R.E";
        org["longName"] = "Sectarian Chosen Elite Privileged To Rule & Exterminate";
        org["isEvil"] = true;
        org["members"] = 218;
        org["honor"] = JSONValue();
        JSONValue officers;
        officers.Push("Ahriman");
        officers.Push("Lilith");
        officers.Push("Suhrim");
        org["officers"] = officers;
        org["allies"] = JSONArray();
        org["sightings"] = JSONObject();
        String jsonString = org.ToString();
        printf("%s\n", jsonString.CString());
        JSONValue parsed;
        bool success = parsed.FromString(jsonString);
        printf("JSON parsing result: %d\n", success);
        printf("Parsed JSON: %s\n", parsed.ToString().CString());
    }

    return 0;
}