// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/String.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

class Graphics;
class ShaderVariation;

/// Description of a shader's vertex attribute.
struct VertexAttribute
{
    /// Name of attribute.
    String name;
    /// Attribute binding point. 
    unsigned location;
    /// Attribute semantic.
    ElementSemantic semantic;
    /// Attribute's semantic index.
    unsigned char index;
};

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

    /// Attempt to link the shaders. Return true on success. Note: the shader program is bound if linking is successful.
    bool Link();

    /// Return the vertex shader.
    ShaderVariation* VertexShader() const;
    /// Return the pixel shader.
    ShaderVariation* PixelShader() const;
    /// Return vertex attribute descriptions.
    const Vector<VertexAttribute>& Attributes() const { return attributes; }
    /// Return combined name of the shader program.
    String FullName() const;

    /// Return the OpenGL shader program. Used internally and should not be called by portable application code.
    unsigned ProgramObject() const { return program; }

private:
    /// OpenGL shader program identifier.
    unsigned program;
    /// Vertex shader.
    WeakPtr<ShaderVariation> vs;
    /// Pixel shader.
    WeakPtr<ShaderVariation> ps;
    /// Vertex attribute semantics and indices.
    Vector<VertexAttribute> attributes;
};

}
