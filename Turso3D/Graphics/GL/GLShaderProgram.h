// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/String.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

class Graphics;
class ShaderVariation;

/// Linked shader program consisting of vertex and pixel shaders.
class ShaderProgram : public GPUObject
{
public:
    /// Construct with shader pointers.
    ShaderProgram(ShaderVariation* vs, ShaderVariation* ps);
    /// Destruct.
    ~ShaderProgram();

    /// Release the linked shader program.
    void Release() override;

    /// Attempt to link the shaders. Return true on success.
    bool Link();

    /// Return the vertex shader.
    ShaderVariation* VertexShader() const;
    /// Return the pixel shader.
    ShaderVariation* PixelShader() const;
    /// Return whether linking attempted.
    bool IsLinked() const { return linked; }

    /// Return the OpenGL shader program. Used internally and should not be called by portable application code.
    unsigned ProgramObject() const { return program; }

private:
    /// OpenGL shader program identifier.
    unsigned program;
    /// Vertex shader.
    WeakPtr<ShaderVariation> vs;
    /// Pixel shader.
    WeakPtr<ShaderVariation> ps;
    /// Linking attempted flag.
    bool linked;
};

}
