// For conditions of distribution and use, see copyright notice in License.txt

#include "../Base/Vector.h"
#include "MemoryBuffer.h"

#include <cstring>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

MemoryBuffer::MemoryBuffer(void* data, size_t numBytes) :
    Stream(data ? numBytes : 0),
    buffer((unsigned char*)data),
    readOnly(false)
{
    SetName("Memory");
}

MemoryBuffer::MemoryBuffer(const void* data, size_t numBytes) :
    Stream(data ? numBytes : 0),
    buffer((unsigned char*)data),
    readOnly(true)
{
    SetName("Memory");
}

MemoryBuffer::MemoryBuffer(Vector<unsigned char>& data) :
    Stream(data.Size()),
    buffer(data.Begin().ptr),
    readOnly(false)
{
}

MemoryBuffer::MemoryBuffer(const Vector<unsigned char>& data) :
    Stream(data.Size()),
    buffer(data.Begin().ptr),
    readOnly(true)
{
}

size_t MemoryBuffer::Read(void* dest, size_t numBytes)
{
    if (numBytes + position > size)
        numBytes = size - position;
    if (!numBytes)
        return 0;
    
    unsigned char* srcPtr = &buffer[position];
    unsigned char* destPtr = (unsigned char*)dest;
    position += numBytes;
    
    size_t copySize = numBytes;
    while (copySize >= sizeof(unsigned))
    {
        *((unsigned*)destPtr) = *((unsigned*)srcPtr);
        srcPtr += sizeof(unsigned);
        destPtr += sizeof(unsigned);
        copySize -= sizeof(unsigned);
    }
    if (copySize & sizeof(unsigned short))
    {
        *((unsigned short*)destPtr) = *((unsigned short*)srcPtr);
        srcPtr += sizeof(unsigned short);
        destPtr += sizeof(unsigned short);
    }
    if (copySize & 1)
        *destPtr = *srcPtr;
    
    return numBytes;
}

size_t MemoryBuffer::Seek(size_t newPosition)
{
    if (newPosition > size)
        newPosition = size;
    
    position = newPosition;
    return position;
}

size_t MemoryBuffer::Write(const void* data, size_t numBytes)
{
    if (numBytes + position > size)
        numBytes = size - position;
    if (!numBytes || readOnly)
        return 0;
    
    unsigned char* srcPtr = (unsigned char*)data;
    unsigned char* destPtr = &buffer[position];
    position += numBytes;
    
    size_t copySize = numBytes;
    while (copySize >= sizeof(unsigned))
    {
        *((unsigned*)destPtr) = *((unsigned*)srcPtr);
        srcPtr += sizeof(unsigned);
        destPtr += sizeof(unsigned);
        copySize -= sizeof(unsigned);
    }
    if (copySize & sizeof(unsigned short))
    {
        *((unsigned short*)destPtr) = *((unsigned short*)srcPtr);
        srcPtr += sizeof(unsigned short);
        destPtr += sizeof(unsigned short);
    }
    if (copySize & 1)
        *destPtr = *srcPtr;
    
    return numBytes;
}

bool MemoryBuffer::IsReadable() const
{
    return buffer != 0;
}

bool MemoryBuffer::IsWritable() const
{
    return buffer && !readOnly;
}

}
