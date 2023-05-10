// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../IO/JSONValue.h"
#include "../IO/StringHash.h"
#include "../Object/Ptr.h"
#include "GraphicsDefs.h"

/// Linked shader program consisting of vertex and fragment shaders.
class ShaderProgram : public RefCounted
{
public:
    /// Construct from shader source code and defines. Graphics subsystem must have been initialized.
    ShaderProgram(const std::string& sourceCode, const std::string& shaderName = JSONValue::emptyString, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    /// Destruct.
    ~ShaderProgram();

    /// Bind for using. No-op if already bound. Return false if program is not successfully linked.
    bool Bind();

    /// Return shader name concatenated from parent shader name and defines.
    const std::string& ShaderName() const { return shaderName; }
    /// Return bitmask of used vertex attributes.
    unsigned Attributes() const { return attributes; }
    /// Return uniform map.
    const std::map<StringHash, int>& Uniforms() const { return uniforms; }
    /// Return uniform location by name or negative if not found.
    int Uniform(const std::string& name) const;
    /// Return uniform location by name or negative if not found.
    int Uniform(const char* name) const;
    /// Return uniform location by name hash or negative if not found.
    int Uniform(StringHash name) const;
    /// Return preset uniform location or negative if not found.
    int Uniform(PresetUniform uniform) const { return presetUniforms[uniform]; }

    /// Return the OpenGL shader program identifier. Zero if not successfully compiled and linked.
    unsigned GLProgram() const { return program; }

private:
    /// Compile & link.
    void Create(const std::string& sourceCode, const std::vector<std::string>& vsDefines, const std::vector<std::string>& fsDefines);
    /// Release the program.
    void Release();

    /// OpenGL shader program identifier.
    unsigned program;
    /// Used vertex attribute bitmask.
    unsigned attributes;
    /// All uniform locations.
    std::map<StringHash, int> uniforms;
    /// Preset uniform locations.
    int presetUniforms[MAX_PRESET_UNIFORMS];
    /// Shader name.
    std::string shaderName;
};
