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
    printf("Size of WeakRefCounted: %d\n", sizeof(WeakRefCounted));

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
        SetRandomSeed(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            vec.Push(Rand());
        int sum = 0;
        int count = 0;
        for (Vector<int>::ConstIterator it = vec.Begin(); it != vec.End(); ++it)
        {
            sum += *it;
            ++count;
        }
        printf("Size: %d capacity: %d\n", vec.Size(), vec.Capacity());
        printf("Counted vector items %d, sum: %d\n", count, sum);
    }
    
    {
        printf("\nTesting List\n");
        List<int> list;
        SetRandomSeed(0);
        for (size_t i = 0; i < NUM_ITEMS; ++i)
            list.Push(Rand());
        int sum = 0;
        int count = 0;
        for (List<int>::ConstIterator it = list.Begin(); it != list.End(); ++it)
        {
            sum += *it;
            ++count;
        }
        printf("Size: %d\n", list.Size());
        printf("Counted list items %d, sum: %d\n", count, sum);
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
        for (HashSet<int>::Iterator it = testHashSet.Begin(); it != testHashSet.End(); ++it)
            sum += *it;
        printf("Set size and sum: %d %d\n", testHashSet.Size(), sum);
        for (unsigned i = 0; i < 32768; ++i)
            testHashSet.Erase(i);
        printf("Set size after erase: %d\n", testHashSet.Size());
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

    return 0;
}