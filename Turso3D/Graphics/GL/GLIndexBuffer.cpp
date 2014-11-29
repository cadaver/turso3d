// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLIndexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

IndexBuffer::IndexBuffer() :
    numIndices(0),
    indexSize(0),
    usage(USAGE_DEFAULT)
{
}

IndexBuffer::~IndexBuffer()
{
    Release();
}

void IndexBuffer::Release()
{
    if (graphics && graphics->GetIndexBuffer() == this)
        graphics->SetIndexBuffer(nullptr);

    /// \todo Destroy OpenGL buffer

    shadowData.Reset();
    numIndices = 0;
    indexSize = 0;
}

bool IndexBuffer::Define(ResourceUsage usage_, size_t numIndices_, size_t indexSize_, bool useShadowData, const void* data)
{
    PROFILE(DefineIndexBuffer);

    Release();

    if (!numIndices_)
    {
        LOGERROR("Can not define index buffer with no indices");
        return false;
    }
    if (usage_ == USAGE_RENDERTARGET)
    {
        LOGERROR("Rendertarget usage is illegal for index buffers");
        return false;
    }
    if (usage_ == USAGE_IMMUTABLE && !data)
    {
        LOGERROR("Immutable index buffer must define initial data");
        return false;
    }
    if (indexSize_ != sizeof(unsigned) && indexSize_ != sizeof(unsigned short))
    {
        LOGERROR("Index buffer index size must be 2 or 4");
        return false;
    }

    numIndices = numIndices_;
    indexSize = indexSize_;
    usage = usage_;

    if (useShadowData)
    {
        shadowData = new unsigned char[numIndices * indexSize];
        if (data)
            memcpy(shadowData.Get(), data, numIndices * indexSize);
    }

    if (graphics && graphics->IsInitialized())
    {
        /// \todo Create OpenGL buffer
    }

    return true;
}

bool IndexBuffer::SetData(size_t firstIndex, size_t numIndices_, const void* data)
{
    PROFILE(UpdateIndexBuffer);

    if (!data)
    {
        LOGERROR("Null source data for updating index buffer");
        return false;
    }
    if (firstIndex + numIndices_ > numIndices)
    {
        LOGERROR("Out of bounds range for updating index buffer");
        return false;
    }
    if (/*buffer && */ usage == USAGE_IMMUTABLE)
    {
        LOGERROR("Can not update immutable index buffer");
        return false;
    }

    if (shadowData)
        memcpy(shadowData.Get() + firstIndex * indexSize, data, numIndices_ * indexSize);

    /// \todo Set data to OpenGL buffer

    return true;
}

}
