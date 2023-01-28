// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Object/AutoPtr.h"
#include "../Object/Ptr.h"
#include "../Math/IntRect.h"
#include "GraphicsDefs.h"

#include <vector>

class RenderBuffer;
class Texture;

/// GPU framebuffer object for rendering.
class FrameBuffer : public RefCounted
{
public:
    /// Construct. %Graphics subsystem must have been initialized.
    FrameBuffer();
    /// Destruct.
    ~FrameBuffer();

    /// Define renderbuffers to render to.
    void Define(RenderBuffer* colorBuffer, RenderBuffer* depthStencilBuffer);
    /// Define textures to render to.
    void Define(Texture* colorTexture, Texture* depthStencilTexture);
    /// Define cube map face to render to.
    void Define(Texture* colorTexture, size_t cubeMapFace, Texture* depthStencilTexture);
    /// Define MRT textures to render to.
    void Define(const std::vector<Texture*>& colorTextures, Texture* depthStencilTexture);
    /// Bind for rendering or for defining.
    void Bind(bool force = false);

    /// Return the OpenGL object identifier.
    unsigned GLBuffer() const { return buffer; }
    /// Blit from one framebuffer to another.
    static void Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter);
    /// Return to backbuffer rendering.
    static void Unbind();

private:
    /// Release the framebuffer object.
    void Release();

    /// OpenGL buffer object identifier.
    unsigned buffer;
};
