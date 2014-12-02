// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../../Base/String.h"
#include "../GPUObject.h"
#include "../GraphicsDefs.h"

namespace Turso3D
{

class Shader;

/// Compiled shader with specific defines.
class TURSO3D_API ShaderVariation : public WeakRefCounted, public GPUObject
{
public:
    /// Construct. Set parent shader and defines but do not compile yet.
    ShaderVariation(Shader* parent, const String& defines);
    /// Destruct.
    ~ShaderVariation();

    /// Release the compiled shader.
    void Release() override;

    /// Compile. Return true on success. No-op that returns previous result if compile already attempted.
    bool Compile();
    
    /// Return the parent shader resource.
    Shader* Parent() const;
    /// Return full name combined from parent resource name and compilation defines.
    String FullName() const;
    /// Return shader stage.
    ShaderStage Stage() const { return stage; }
    /// Return vertex element hash code for vertex shaders.
    unsigned ElementHash() const { return elementHash; }
    /// Return whether compile attempted.
    bool IsCompiled() const { return compiled; }

    /// Return the D3D11 shader byte blob. Null if not compiled yet or compile failed. Used internally and should not be called by portable application code.
    void* BlobObject() const { return blob; }
    /// Return the D3D11 shader. Null if not compiled yet or compile failed. Used internally and should not be called by portable application code.
    void* ShaderObject() { return shader; }

private:
    /// Parent shader resource.
    WeakPtr<Shader> parent;
    /// Shader stage.
    ShaderStage stage;
    /// Compilation defines.
    String defines;
    /// D3D11 shader byte blob.
    void* blob;
    /// D3D11 shader.
    void* shader;
    /// Vertex shader element hash code.
    unsigned elementHash;
    /// Compile attempted flag.
    bool compiled;
};

}