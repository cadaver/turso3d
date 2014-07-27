// For conditions of distribution and use, see copyright notice in License.txt

#include "AutoPtr.h"
#include "SharedPtr.h"
#include "Vector.h"
#include "WeakPtr.h"
#include "DebugNew.h"

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

int main()
{
    #ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    #endif
    
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
        for (int i = 0; i < 1000000; ++i)
            vec.Push(rand());
        int sum = 0;
        for (Vector<int>::ConstIterator i = vec.Begin(); i != vec.End(); ++i)
            sum += *i;
        printf("After 1000000 pushes size: %d capacity: %d\n", vec.Size(), vec.Capacity());
    }

    return 0;
}