// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Resource/Resource.h"
#include "GraphicsDefs.h"

namespace Turso3D
{

class ShaderVariation;

/// %Shader resource. Defines either vertex or pixel shader source code, from which variations can be compiled by specifying defines.
class Shader : public Resource
{
    OBJECT(Shader);

public:
    /// Construct.
    Shader();
    /// Destruct.
    ~Shader();

    /// Register object factory.
    static void RegisterObject();

    /// Load shader code from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    /// Finish shader loading in the main thread. Return true on success.
    bool EndLoad() override;

    /// Define shader stage and source code. All existing variations are destroyed.
    void Define(ShaderStage stage, const String& code);
    /// Create and return a variation with defines, eg. "PERPIXEL NORMALMAP NUMLIGHTS=4". Existing variation is returned if possible. Variations should be cached to avoid repeated query.
    ShaderVariation* CreateVariation(const String& defines = String::EMPTY);
    
    /// Return shader stage.
    ShaderStage Stage() const { return stage; }
    /// Return shader source code.
    const String& SourceCode() const { return sourceCode; }

    /// Sort the defines and strip extra spaces to prevent creation of unnecessary duplicate shader variations. When requesting variations, the defines should preferably be normalized already to save time.
    static String NormalizeDefines(const String& defines);

private:
    /// Process include statements in the shader source code recursively. Return true if successful.
    bool ProcessIncludes(String& code, Stream& source);

    /// %Shader variations.
    HashMap<StringHash, AutoPtr<ShaderVariation> > variations;
    /// %Shader stage.
    ShaderStage stage;
    /// %Shader source code.
    String sourceCode;
};

}
