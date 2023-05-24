// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Color.h"
#include "../Math/IntRect.h"
#include "../Resource/Image.h"
#include "GraphicsDefs.h"

/// GPU renderbuffer object for rendering and blitting, that cannot be sampled as a texture.
class RenderBuffer : public RefCounted
{
public:
    /// Construct. Graphics subsystem must have been initialized.
    RenderBuffer();
    /// Destruct.
    ~RenderBuffer();

    /// Define renderbuffer type and dimensions.
    bool Define(const IntVector2& size, ImageFormat format, int multisample = 1);

    /// Return dimensions.
    const IntVector2& Size() const { return size; }
    /// Return width.
    int Width() const { return size.x; }
    /// Return height.
    int Height() const { return size.y; }
    ImageFormat Format() const { return format; }
    /// Return multisampling level, or 1 if not multisampled.
    int Multisample() const { return multisample; }

    /// Return the OpenGL buffer identifier.
    unsigned GLBuffer() const { return buffer; }

private:
    /// Release the renderbuffer.
    void Release();

    /// OpenGL object identifier.
    unsigned buffer;
    /// Texture dimensions in pixels.
    IntVector2 size;
    /// Image format.
    ImageFormat format;
    /// Multisampling level.
    int multisample;
};
