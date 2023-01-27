// For conditions of distribution and use, see copyright notice in License.txt

#include "TimeUtils.h"
#include "../IO/StringUtils.h"

#include <ctime>

#ifdef _WIN32
#include <Windows.h>
#include <MMSystem.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

std::string TimeStamp()
{
    time_t sysTime;
    time(&sysTime);

    std::string ret(ctime(&sysTime));
    return Replace(ret, "\n", "");
}

unsigned CurrentTime()
{
    return (unsigned)time(NULL);
}
