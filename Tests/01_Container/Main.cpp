// For conditions of distribution and use, see copyright notice in License.txt

#include "Ptr.h"

#include <cstdio>

class Test
{
public:
    Test() :
        value(1)
    {
        printf("Test constructed\n");
    }

    ~Test()
    {
        printf("Test destroyed\n");
    }

    void Function()
    {
        ++value;
    }

    int value;
};

int main()
{
    {
        printf("Testing AutoPtr assignment\n");
        AutoPtr<Test> ptr1(new Test);
        AutoPtr<Test> ptr2;
        ptr2 = ptr1;
    }

    {
        printf("Testing AutoPtr copy construction\n");
        AutoPtr<Test> ptr1(new Test);
        AutoPtr<Test> ptr2(ptr1);
    }

    {
        printf("Testing AutoPtr detaching\n");
        AutoPtr<Test> ptr1(new Test);
        // We now have an intentional memory leak
        ptr1.Detach();
    }

    return 0;
}