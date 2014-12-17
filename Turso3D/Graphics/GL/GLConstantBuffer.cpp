// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../../Math/Color.h"
#include "../../Math/Matrix3.h"
#include "../../Math/Matrix3x4.h"
#include "GLConstantBuffer.h"
#include "GLGraphics.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ConstantBuffer::ConstantBuffer() :
    buffer(0),
    byteSize(0),
    usage(USAGE_DEFAULT),
    dirty(false)
{
}

ConstantBuffer::~ConstantBuffer()
{
    Release();
}

void ConstantBuffer::Release()
{
    if (graphics)
    {
        for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
        {
            for (size_t j = 0; j < MAX_CONSTANT_BUFFERS; ++j)
            {
                if (graphics->GetConstantBuffer((ShaderStage)i, j) == this)
                    graphics->SetConstantBuffer((ShaderStage)i, j, 0);
            }
        }
    }

    if (buffer)
    {
        glDeleteBuffers(1, &buffer);
        buffer = 0;
    }
}

void ConstantBuffer::Recreate()
{
    if (constants.Size())
    {
        // Make a copy of the current constants, as they are passed by reference and manipulated by Define()
        Vector<Constant> srcConstants = constants;
        Define(usage, srcConstants);
        Apply();
    }
}

bool ConstantBuffer::Apply()
{
    if (usage == USAGE_IMMUTABLE)
    {
        if (!buffer)
            return Create(shadowData.Get());
        else
        {
            LOGERROR("Apply can only be called once on an immutable constant buffer");
            return false;
        }
    }

    if (!dirty)
        return true;

    if (buffer)
    {
        /// \todo Investigate whether mapping or glBufferData / glBufferSubData gives better performance
        glBindBuffer(GL_UNIFORM_BUFFER, buffer);
        void* mappedData = glMapBuffer(GL_UNIFORM_BUFFER, GL_WRITE_ONLY);
        if (mappedData)
            memcpy(mappedData, shadowData.Get(), byteSize);
        else
            return false;
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    }

    dirty = false;
    return true;
}

bool ConstantBuffer::Create(const void* data)
{
    dirty = false;

    if (graphics && graphics->IsInitialized())
    {
        glGenBuffers(1, &buffer);
        if (!buffer)
        {
            LOGERROR("Failed to create constant buffer");
            return false;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, buffer);
        glBufferData(GL_UNIFORM_BUFFER, byteSize, data, usage == USAGE_DYNAMIC ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    }

    return true;
}

}
