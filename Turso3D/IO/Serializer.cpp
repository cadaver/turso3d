// For conditions of distribution and use, see copyright notice in License.txt

#include "../Math/BoundingBox.h"
#include "Serializer.h"
#include "Variant.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const float q = 32767.0f;

Serializer::~Serializer()
{
}

bool Serializer::WriteInt(int value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteShort(short value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteByte(signed char value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUInt(unsigned value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUShort(unsigned short value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteUByte(unsigned char value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteBool(bool value)
{
    return WriteUByte(value ? 1 : 0) == 1;
}

bool Serializer::WriteFloat(float value)
{
    return Write(&value, sizeof value) == sizeof value;
}

bool Serializer::WriteIntRect(const IntRect& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteIntVector2(const IntVector2& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteRect(const Rect& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteVector2(const Vector2& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteVector3(const Vector3& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WritePackedVector3(const Vector3& value, float maxAbsCoord)
{
    short coords[3];
    float v = 32767.0f / maxAbsCoord;
    
    coords[0] = (short)(Clamp(value.x, -maxAbsCoord, maxAbsCoord) * v + 0.5f);
    coords[1] = (short)(Clamp(value.y, -maxAbsCoord, maxAbsCoord) * v + 0.5f);
    coords[2] = (short)(Clamp(value.z, -maxAbsCoord, maxAbsCoord) * v + 0.5f);
    return Write(&coords[0], sizeof coords) == sizeof coords;
}

bool Serializer::WriteVector4(const Vector4& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteQuaternion(const Quaternion& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WritePackedQuaternion(const Quaternion& value)
{
    short coords[4];
    Quaternion norm = value.Normalized();

    coords[0] = (short)(Clamp(norm.w, -1.0f, 1.0f) * q + 0.5f);
    coords[1] = (short)(Clamp(norm.x, -1.0f, 1.0f) * q + 0.5f);
    coords[2] = (short)(Clamp(norm.y, -1.0f, 1.0f) * q + 0.5f);
    coords[3] = (short)(Clamp(norm.z, -1.0f, 1.0f) * q + 0.5f);
    return Write(&coords[0], sizeof coords) == sizeof value;
}

bool Serializer::WriteMatrix3(const Matrix3& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteMatrix3x4(const Matrix3x4& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteMatrix4(const Matrix4& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteColor(const Color& value)
{
    return Write(value.Data(), sizeof value) == sizeof value;
}

bool Serializer::WriteBoundingBox(const BoundingBox& value)
{
    bool success = true;
    success &= WriteVector3(value.min);
    success &= WriteVector3(value.max);
    return success;
}

bool Serializer::WriteString(const String& value, bool nullTerminate)
{
    return WriteString(value.CString(), nullTerminate);
}

bool Serializer::WriteString(const char* value, bool nullTerminate)
{
    // Count length to the first zero, because ReadString() does the same
    size_t length = String::CStringLength(value);
    if (nullTerminate)
        return Write(value, length + 1) == length + 1;
    else
        return Write(value, length) == length;
}

bool Serializer::WriteFileID(const String& value)
{
    bool success = true;
    unsigned length = Min((int)value.Length(), 4);
    
    success &= Write(value.CString(), length) == length;
    for (size_t i = value.Length(); i < 4; ++i)
        success &= WriteByte(' ');
    return success;
}

bool Serializer::WriteStringHash(const StringHash& value)
{
    return WriteUInt(value.Value());
}

bool Serializer::WriteBuffer(const Vector<unsigned char>& value)
{
    bool success = true;
    size_t numBytes = value.Size();
    
    success &= WriteVLE((unsigned)numBytes);
    if (numBytes)
        success &= Write(&value[0], numBytes) == numBytes;
    return success;
}

bool Serializer::WriteResourceRef(const ResourceRef& value)
{
    bool success = true;
    success &= WriteStringHash(value.type);
    success &= WriteString(value.name);
    return success;
}

bool Serializer::WriteResourceRefList(const ResourceRefList& value)
{
    bool success = true;
    
    success &= WriteStringHash(value.type);
    success &= WriteVLE((unsigned)value.names.Size());
    for (size_t i = 0; i < value.names.Size(); ++i)
        success &= WriteString(value.names[i]);
    
    return success;
}

bool Serializer::WriteVariant(const Variant& value)
{
    bool success = true;
    VariantType type = value.Type();
    
    success &= WriteUByte((unsigned char)type);
    success &= WriteVariantData(value);
    return success;
}

bool Serializer::WriteVariantData(const Variant& value)
{
    switch (value.Type())
    {
    case VAR_NONE:
        return true;
        
    case VAR_INT:
        return WriteInt(value.AsInt());
        
    case VAR_BOOL:
        return WriteBool(value.AsBool());
        
    case VAR_FLOAT:
        return WriteFloat(value.AsFloat());
        
    case VAR_VECTOR2:
        return WriteVector2(value.AsVector2());
        
    case VAR_VECTOR3:
        return WriteVector3(value.AsVector3());
        
    case VAR_VECTOR4:
        return WriteVector4(value.AsVector4());
        
    case VAR_QUATERNION:
        return WriteQuaternion(value.AsQuaternion());
        
    case VAR_COLOR:
        return WriteColor(value.AsColor());
        
    case VAR_STRING:
        return WriteString(value.AsString());
        
    case VAR_BUFFER:
        return WriteBuffer(value.AsBuffer());
        
        // Serializing pointers is not supported. Write null
    case VAR_VOIDPTR:
    case VAR_PTR:
        return WriteUInt(0);
        
    case VAR_RESOURCEREF:
        return WriteResourceRef(value.AsResourceRef());
        
    case VAR_RESOURCEREFLIST:
        return WriteResourceRefList(value.AsResourceRefList());
        
    case VAR_VARIANTVECTOR:
        return WriteVariantVector(value.AsVariantVector());
        
    case VAR_VARIANTMAP:
        return WriteVariantMap(value.AsVariantMap());
        
    case VAR_INTRECT:
        return WriteIntRect(value.AsIntRect());
        
    case VAR_INTVECTOR2:
        return WriteIntVector2(value.AsIntVector2());
        
    case VAR_MATRIX3:
        return WriteMatrix3(value.AsMatrix3());
        
    case VAR_MATRIX3X4:
        return WriteMatrix3x4(value.AsMatrix3x4());
        
    case VAR_MATRIX4:
        return WriteMatrix4(value.AsMatrix4());
        
    default:
        return false;
    }
}

bool Serializer::WriteVariantVector(const VariantVector& value)
{
    bool success = true;
    success &= WriteVLE((unsigned)value.Size());
    for (VariantVector::ConstIterator i = value.Begin(); i != value.End(); ++i)
        success &= WriteVariant(*i);
    return success;
}

bool Serializer::WriteVariantMap(const VariantMap& value)
{
    bool success = true;
    success &= WriteVLE((unsigned)value.Size());
    for (VariantMap::ConstIterator i = value.Begin(); i != value.End(); ++i)
    {
        WriteStringHash(i->first);
        WriteVariant(i->second);
    }
    return success;
}

bool Serializer::WriteVLE(unsigned value)
{
    unsigned char data[4];
    
    if (value < 0x80)
        return WriteUByte((unsigned char)value);
    else if (value < 0x4000)
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)(value >> 7);
        return Write(data, 2) == 2;
    }
    else if (value < 0x200000)
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)((value >> 7) | 0x80);
        data[2] = (unsigned char)(value >> 14);
        return Write(data, 3) == 3;
    }
    else
    {
        data[0] = (unsigned char)value | 0x80;
        data[1] = (unsigned char)((value >> 7) | 0x80);
        data[2] = (unsigned char)((value >> 14) | 0x80);
        data[3] = (unsigned char)(value >> 21);
        return Write(data, 4) == 4;
    }
}

bool Serializer::WriteLine(const String& value)
{
    bool success = true;
    success &= Write(value.CString(), value.Length()) == value.Length();
    success &= WriteUByte(13);
    success &= WriteUByte(10);
    return success;
}

}
