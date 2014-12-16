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

class TestRefCounted: public RefCounted
{
public:
    TestRefCounted()
    {
        printf("TestRefCounted constructed\n");
    }

    ~TestRefCounted()
    {
        printf("TestRefCounted destroyed\n");
    }
};

const size_t NUM_ITEMS = 10000;

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
    printf("Size of String: %d\n", sizeof(String));
    printf("Size of Vector: %d\n", sizeof(Vector<int>));
    printf("Size of List: %d\n", sizeof(List<int>));
    printf("Size of HashMap: %d\n", sizeof(HashMap<int, int>));
    printf("Size of RefCounted: %d\n", sizeof(RefCounted));

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
        printf("\nTesting AutoPtr inside a vector\n");
        Vector<AutoPtr<Test> > vec;
        printf("Filling vector\n");
        for (size_t i = 0; i < 4; ++i)
            vec.Push(new Test());
        printf("Clearing vector\n");
        vec.Clear();
    }
    
    {
        printf("\nTesting Ptr\n");
        Ptr<TestRefCounted> ptr1(new TestRefCounted);
        Ptr<TestRefCounted> ptr2(ptr1);
        printf("Number of refs: %d\n", ptr1.Refs());
    }
    
    {
        printf("\nTesting WeakPtr\n");
        TestRefCounted* object = new TestRefCounted;
        WeakPtr<TestRefCounted> ptr1(object);
        WeakPtr<TestRefCounted> ptr2(ptr1);
        printf("Number of weak refs: %d expired: %d\n", ptr1.WeakRefs(), ptr1.IsExpired());
        ptr2.Reset();
        delete object;
        printf("Number of weak refs: %d expired: %d\n", ptr1.WeakRefs(), ptr1.IsExpired());
    }
    
    {
        printf("\nTesting Vector\n");
        HiresTimer t;
        Vector<int> vec;
        SetRandomSeed(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            vec.Push(Rand());
        int sum = 0;
        int count = 0;
        for (auto it = vec.Begin(); it != vec.End(); ++it)
        {
            sum += *it;
            ++count;
        }
        int usec = (int)t.ElapsedUSec();
        printf("Size: %d capacity: %d\n", vec.Size(), vec.Capacity());
        printf("Counted vector items %d, sum: %d\n", count, sum);
        printf("Processing took %d usec\n", usec);
    }

    {
        printf("\nTesting List\n");
        HiresTimer t;
        List<int> list;
        SetRandomSeed(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            list.Push(Rand());
        int sum = 0;
        int count = 0;
        for (auto it = list.Begin(); it != list.End(); ++it)
        {
            sum += *it;
            ++count;
        }
        int usec = (int)t.ElapsedUSec();
        printf("Size: %d\n", list.Size());
        printf("Counted list items %d, sum: %d\n", count, sum);
        printf("Processing took %d usec\n", usec);

        printf("\nTesting List insertion\n");
        List<int> list2;
        List<int> list3;
        for (int i = 0; i < 10; ++i)
            list3.Push(i);
        list2.Insert(list2.End(), list3);
        for (auto it = list2.Begin(); it != list2.End(); ++it)
            printf("%d ", *it);
        printf("\n");
    }
    
    {
        printf("\nTesting String\n");
        HiresTimer t;
        String test;
        for (size_t i = 0; i < NUM_ITEMS/4; ++i)
            test += "Test";
        String test2;
        test2.AppendWithFormat("Size: %d capacity: %d\n", test.Length(), test.Capacity());
        printf(test2.CString());
        test2 = test2.ToUpper();
        printf(test2.CString());
        test2.Replace("SIZE:", "LENGTH:");
        printf(test2.CString());
        int usec = (int)t.ElapsedUSec();
        printf("Processing took %d usec\n", usec);
    }
    
    {
        printf("\nTesting HashSet\n");
        HiresTimer t;
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
        for (auto it = testHashSet.Begin(); it != testHashSet.End(); ++it)
            sum += *it;
        int usec = (int)t.ElapsedUSec();
        printf("Set size and sum: %d %d\n", testHashSet.Size(), sum);
        printf("Processing took %d usec\n", usec);
    }

    {
        printf("\nTesting HashMap\n");
        HashMap<int, int> testHashMap;

        for (int i = 0; i < 10; ++i)
            testHashMap.Insert(MakePair(i, rand() & 32767));

        printf("Keys: ");
        Vector<int> keys = testHashMap.Keys();
        for (size_t i = 0; i < keys.Size(); ++i)
            printf("%d ", keys[i]);
        printf("\n");
        printf("Values: ");
        Vector<int> values = testHashMap.Values();
        for (size_t i = 0; i < values.Size(); ++i)
            printf("%d ", values[i]);
        printf("\n");
    }

    return 0;
}