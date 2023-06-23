// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Resource/Resource.h"
#include "GraphicsDefs.h"

class ShaderProgram;

/// %Shader resource. Defines shader source code, from which shader programs can be compiled & linked by specifying defines.
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

    /// Define shader from source code. All existing variations are destroyed.
    void Define(const std::string& code);
    /// Create and return a shader program with defines. Existing program is returned if possible. Variations should be cached to avoid repeated query.
    ShaderProgram* CreateProgram(const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    
    /// Return shader source code.
    const std::string& SourceCode() const { return sourceCode; }

private:
    /// Sort the defines and strip extra spaces to prevent creation of unnecessary duplicate shader variations.
    std::string NormalizeDefines(const std::string& defines);
    /// Process include statements in the shader source code recursively. Return true if successful.
    bool ProcessIncludes(std::string& code, Stream& source);

    /// %Shader programs.
    std::map<std::pair<StringHash, StringHash>, SharedPtr<ShaderProgram> > programs;
    /// %Shader source code.
    std::string sourceCode;
};
