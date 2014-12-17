// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../IO/JSONValue.h"
#include "../Math/Matrix3x4.h"
#include "../Object/Attribute.h"
#include "ConstantBuffer.h"
#include "GraphicsDefs.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

static const AttributeType elementToAttribute[] =
{
    ATTR_INT,
    ATTR_FLOAT,
    ATTR_VECTOR2,
    ATTR_VECTOR3,
    ATTR_VECTOR4,
    MAX_ATTR_TYPES,
    ATTR_MATRIX3X4,
    ATTR_MATRIX4,
    MAX_ATTR_TYPES
};

bool ConstantBuffer::LoadJSON(const JSONValue& src)
{
    ResourceUsage usage_ = USAGE_DEFAULT;
    if (src.Contains("usage"))
        usage_ = (ResourceUsage)String::ListIndex(src["usage"].GetString().CString(), resourceUsageNames, USAGE_DEFAULT);

    Vector<Constant> constants_;

    const JSONValue& jsonConstants = src["constants"];
    for (size_t i = 0; i < jsonConstants.Size(); ++i)
    {
        const JSONValue& jsonConstant = jsonConstants[i];
        const String& type = jsonConstant["type"].GetString();

        Constant newConstant;
        newConstant.name = jsonConstant["name"].GetString();
        newConstant.type = (ElementType)String::ListIndex(type.CString(), elementTypeNames, MAX_ELEMENT_TYPES);
        if (newConstant.type == MAX_ELEMENT_TYPES)
        {
            LOGERRORF("Unknown element type %s in constant buffer JSON", type.CString());
            break;
        }
        if (jsonConstant.Contains("numElements"))
            newConstant.numElements = (int)jsonConstant["numElements"].GetNumber();

        constants_.Push(newConstant);
    }

    if (!Define(usage_, constants_))
        return false;

    for (size_t i = 0; i < constants.Size(); ++i)
    {
        const Constant& constant = constants[i];
        AttributeType attrType = elementToAttribute[constant.type];

        const JSONValue& value = jsonConstants[i]["value"];
        unsigned char* dest = const_cast<unsigned char*>(ConstantData(i));
        if (value.IsArray())
        {
            for (size_t j = 0; j < value.Size(); ++j)
                Attribute::FromJSON(attrType, dest + j * constant.elementSize, value[j]);
        }
        else
            Attribute::FromJSON(attrType, dest, value);
    }

    dirty = true;
    Apply();
    return true;
}

void ConstantBuffer::SaveJSON(JSONValue& dest)
{
    dest.SetEmptyObject();
    dest["usage"] = resourceUsageNames[usage];
    dest["constants"].SetEmptyArray();

    for (size_t i = 0; i < constants.Size(); ++i)
    {
        const Constant& constant = constants[i];
        AttributeType attrType = elementToAttribute[constant.type];

        JSONValue jsonConstant;
        jsonConstant["name"] = constant.name;
        jsonConstant["type"] = elementTypeNames[constant.type];
        if (constant.numElements != 1)
            jsonConstant["numElements"] = (int)constant.numElements;

        const unsigned char* source = ConstantData(i);
        
        if (constant.numElements == 1)
            Attribute::ToJSON(attrType, jsonConstant["value"], source);
        else
        {
            jsonConstant["value"].Resize(constant.numElements);
            for (size_t j = 0; j < constant.numElements; ++j)
                Attribute::ToJSON(attrType, jsonConstant["value"][j], source + j * constant.elementSize);
        }

        dest["constants"].Push(jsonConstant);
    }
}

bool ConstantBuffer::Define(ResourceUsage usage_, const Vector<Constant>& srcConstants)
{
    return Define(usage_, srcConstants.Size(), srcConstants.Size() ? &srcConstants[0] : nullptr);
}

bool ConstantBuffer::Define(ResourceUsage usage_, size_t numConstants, const Constant* srcConstants)
{
    PROFILE(DefineConstantBuffer);
    
    Release();
    
    if (!numConstants)
    {
        LOGERROR("Can not define constant buffer with no constants");
        return false;
    }
    if (usage_ == USAGE_RENDERTARGET)
    {
        LOGERROR("Rendertarget usage is illegal for constant buffers");
        return false;
    }
    
    constants.Clear();
    byteSize = 0;
    usage = usage_;
    
    while (numConstants--)
    {
        if (srcConstants->type == ELEM_UBYTE4)
        {
            LOGERROR("UBYTE4 type is not supported in constant buffers");
            constants.Clear();
            byteSize = 0;
            return false;
        }

        Constant newConstant;
        newConstant.type = srcConstants->type;
        newConstant.name = srcConstants->name;
        newConstant.numElements = srcConstants->numElements;
        newConstant.elementSize = elementSizes[newConstant.type];
        // If element crosses 16 byte boundary or is larger than 16 bytes, align to next 16 bytes
        if ((newConstant.elementSize <= 16 && ((byteSize + newConstant.elementSize - 1) >> 4) != (byteSize >> 4)) ||
            (newConstant.elementSize > 16 && (byteSize & 15)))
            byteSize += 16 - (byteSize & 15);
        newConstant.offset = byteSize;
        constants.Push(newConstant);
        
        byteSize += newConstant.elementSize * newConstant.numElements;
        ++srcConstants;
    }

    // Align the final buffer size to a multiple of 16 bytes
    if (byteSize & 15)
        byteSize += 16 - (byteSize & 15);
    
    shadowData = new unsigned char[byteSize];

    if (usage != USAGE_IMMUTABLE)
        return Create();
    else
        return true;
}

bool ConstantBuffer::SetConstant(size_t index, const void* data, size_t numElements)
{
    if (index >= constants.Size())
        return false;
    
    const Constant& constant = constants[index];
    
    if (!numElements || numElements > constant.numElements)
        numElements = constant.numElements;
    
    memcpy(shadowData.Get() + constant.offset, data, numElements * constant.elementSize);
    dirty = true;
    return true;
}

bool ConstantBuffer::SetConstant(const String& name, const void* data, size_t numElements)
{
    return SetConstant(name.CString(), data, numElements);
}

bool ConstantBuffer::SetConstant(const char* name, const void* data, size_t numElements)
{
    for (size_t i = 0; i < constants.Size(); ++i)
    {
        if (constants[i].name == name)
            return SetConstant(i, data, numElements);
    }
    
    return false;
}

size_t ConstantBuffer::FindConstantIndex(const String& name) const
{
    return FindConstantIndex(name.CString());
}

size_t ConstantBuffer::FindConstantIndex(const char* name) const
{
    for (size_t i = 0; i < constants.Size(); ++i)
    {
        if (constants[i].name == name)
            return i;
    }

    return NPOS;
}

const unsigned char* ConstantBuffer::ConstantData(size_t index) const
{
    return index < constants.Size() ? shadowData.Get() + constants[index].offset : nullptr;
}

const unsigned char* ConstantBuffer::ConstantData(const String& name) const
{
    return ConstantData(FindConstantIndex(name));
}

const unsigned char* ConstantBuffer::ConstantData(const char* name) const
{
    return ConstantData(FindConstantIndex(name));
}

}
