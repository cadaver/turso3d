// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/BoundingBox.h"
#include "Variant.h"

namespace Turso3D
{

/// Abstract stream for reading.
class TURSO3D_API Deserializer
{
public:
    /// Construct with zero size.
    Deserializer();
    /// Construct with defined size.
    Deserializer(size_t size);
    /// Destruct.
    virtual ~Deserializer();
    
    /// Read bytes from the stream. Return number of bytes actually read.
    virtual size_t Read(void* dest, size_t numBytes) = 0;
    /// Set position in bytes from the beginning of the stream.
    virtual size_t Seek(size_t position) = 0;
    /// Return name of the stream.
    virtual const String& Name() const;
    /// Return current position in bytes.
    size_t Position() const { return position; }
    /// Return size in bytes.
    size_t Size() const { return size; }
    /// Return whether the end of stream has been reached.
    bool IsEof() const { return position >= size; }
    
    /// Read a 32-bit integer.
    int ReadInt();
    /// Read a 16-bit integer.
    short ReadShort();
    /// Read an 8-bit integer.
    signed char ReadByte();
    /// Read a 32-bit unsigned integer.
    unsigned ReadUInt();
    /// Read a 16-bit unsigned integer.
    unsigned short ReadUShort();
    /// Read an 8-bit unsigned integer.
    unsigned char ReadUByte();
    /// Read a bool.
    bool ReadBool();
    /// Read a float.
    float ReadFloat();
    /// Read an IntRect.
    IntRect ReadIntRect();
    /// Read an IntVector2.
    IntVector2 ReadIntVector2();
    /// Read a Rect.
    Rect ReadRect();
    /// Read a Vector2.
    Vector2 ReadVector2();
    /// Read a Vector3.
    Vector3 ReadVector3();
    /// Read a Vector3 packed into 3 x 16 bits with the specified maximum absolute range.
    Vector3 ReadPackedVector3(float maxAbsCoord);
    /// Read a Vector4.
    Vector4 ReadVector4();
    /// Read a quaternion.
    Quaternion ReadQuaternion();
    /// Read a quaternion with each component packed in 16 bits.
    Quaternion ReadPackedQuaternion();
    /// Read a Matrix3.
    Matrix3 ReadMatrix3();
    /// Read a Matrix3x4.
    Matrix3x4 ReadMatrix3x4();
    /// Read a Matrix4.
    Matrix4 ReadMatrix4();
    /// Read a color.
    Color ReadColor();
    /// Read a bounding box.
    BoundingBox ReadBoundingBox();
    /// Read a null-terminated string.
    String ReadString();
    /// Read a four-letter file ID.
    String ReadFileID();
    /// Read a 32-bit StringHash.
    StringHash ReadStringHash();
    /// Read a buffer with size encoded as VLE.
    Vector<unsigned char> ReadBuffer();
    /// Read a resource reference.
    ResourceRef ReadResourceRef();
    /// Read a resource reference list.
    ResourceRefList ReadResourceRefList();
    /// Read a variant.
    Variant ReadVariant();
    /// Read a variant whose type is already known.
    Variant ReadVariant(VariantType type);
    /// Read a variant vector.
    VariantVector ReadVariantVector();
    /// Read a variant map.
    VariantMap ReadVariantMap();
    /// Read a variable-length encoded unsigned integer, which can use 29 bits maximum.
    unsigned ReadVLE();
    /// Read a text line.
    String ReadLine();
    
protected:
    /// Stream position.
    size_t position;
    /// Stream size.
    size_t  size;
};

}
