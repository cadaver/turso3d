// For conditions of distribution and use, see copyright notice in License.txt

#include "VectorBuffer.h"

#include <cstring>

#include "../Debug/DebugNew.h"

namespace Turso3D
{

VectorBuffer::VectorBuffer()
{
}

VectorBuffer::VectorBuffer(const Vector<unsigned char>& data)
{
    SetData(data);
}

VectorBuffer::VectorBuffer(const void* data, size_t numBytes)
{
    SetData(data, numBytes);
}

VectorBuffer::VectorBuffer(Deserializer& source, size_t numBytes)
{
    SetData(source, numBytes);
}

size_t VectorBuffer::Read(void* dest, size_t numBytes)
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
    
    return size;
}

size_t VectorBuffer::Seek(size_t newPosition)
{
    if (newPosition > size)
        newPosition = size;
    
    position = newPosition;
    return position;
}

size_t VectorBuffer::Write(const void* data, size_t numBytes)
{
    if (!numBytes)
        return 0;
    
    // Expand the buffer if necessary
    if (numBytes + position > size)
    {
        size = position + numBytes;
        buffer.Resize(size);
    }
    
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

void VectorBuffer::SetData(const Vector<unsigned char>& data)
{
    buffer = data;
    position = 0;
    size = data.Size();
}

void VectorBuffer::SetData(const void* data, size_t numBytes)
{
    if (!data)
        numBytes = 0;
    
    buffer.Resize(numBytes);
    if (numBytes)
        memcpy(&buffer[0], data, numBytes);
    
    position = 0;
    size = numBytes;
}

void VectorBuffer::SetData(Deserializer& source, size_t numBytes)
{
    buffer.Resize(numBytes);
    size_t actualSize = source.Read(&buffer[0], numBytes);
    if (actualSize != numBytes)
        buffer.Resize(actualSize);
    
    position = 0;
    size = actualSize;
}

void VectorBuffer::Clear()
{
    buffer.Clear();
    position = 0;
    size = 0;
}

void VectorBuffer::Resize(size_t newSize)
{
    buffer.Resize(newSize);
    size = newSize;
    if (position > size)
        position = size;
}

}
