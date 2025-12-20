// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "../Math/IntRect.h"
#include "GraphicsDefs.h"

#include <vector>

class RenderBuffer;
class Texture;

/// GPU framebuffer object for rendering. Combines color and depth-stencil textures or buffers.
class FrameBuffer : public RefCounted
{
public:
    /// Construct. Graphics subsystem must have been initialized.
    FrameBuffer();
    /// Destruct.
    ~FrameBuffer();

    /// Create GPU framebuffer and define renderbuffers to render to. Leave buffer(s) null for color-only or depth-only rendering. Return true on success.
    bool Define(RenderBuffer* colorBuffer, RenderBuffer* depthStencilBuffer);
    /// Create GPU framebuffer and define textures to render to. Leave texture(s) null for color-only or depth-only rendering. Return true on success.
    bool Define(Texture* colorTexture, Texture* depthStencilTexture);
    /// Create GPU framebuffer and define cube map face to render to. Return true on success.
    bool Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture);
    /// Create GPU framebuffer and define MRT textures to render to. Return true on success.
    bool Define(const std::vector<Texture*>& colorTextures, Texture* depthStencilTexture);
    /// Bind as draw framebuffer. No-op if already bound. Used also when defining.
    void Bind();
    /// Destroy the GPU framebuffer.
    void Destroy();

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }

    /// Bind separate framebuffers for drawing and reading.
    static void Bind(FrameBuffer* draw, FrameBuffer* read);
    /// Unbind the current draw and read framebuffers and return to backbuffer rendering.
    static void Unbind();

private:
    /// Create the GPU framebuffer object if does not yet exist, and bind for defining. Return true on success.
    bool Create();

    /// OpenGL buffer object identifier.
    unsigned buffer;
};
