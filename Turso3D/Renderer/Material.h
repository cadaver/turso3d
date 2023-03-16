// For conditions of distribution and use, see copyright notice in License.txt

#pragma once

#include "../Math/Vector4.h"
#include "../Object/AutoPtr.h"
#include "../Graphics/GraphicsDefs.h"
#include "../Graphics/Shader.h"
#include "../Graphics/ShaderProgram.h"
#include "../Resource/Resource.h"

#include <set>

class JSONFile;
class JSONValue;
class Material;
class Texture;

enum PassType
{
    PASS_SHADOW = 0,
    PASS_OPAQUE,
    PASS_ALPHA,
    MAX_PASS_TYPES
};

/// Shader program bits
static const unsigned SP_STATIC = 0x0;
static const unsigned SP_SKINNED = 0x1;
static const unsigned SP_INSTANCED = 0x2;
static const unsigned SP_CUSTOMGEOM = 0x3;
static const unsigned SP_GEOMETRYBITS = 0x3;

static const size_t MAX_SHADER_VARIATIONS = 4;

/// Render pass, which defines render state and shaders. A material may define several of these.
class Pass : public RefCounted
{
public:
    /// Construct.
    Pass(Material* parent);
    /// Destruct.
    ~Pass();

    /// Load from JSON data.
    void LoadJSON(const JSONValue& source);
    /// Set shader and shader defines. Existing shader programs will be cleared.
    void SetShader(Shader* shader, const std::string& vsDefines = JSONValue::emptyString, const std::string& fsDefines = JSONValue::emptyString);
    /// Reset existing shader programs.
    void ResetShaderPrograms();
    /// Set render state.
    void SetRenderState(BlendMode blendMode, CompareMode depthTest = CMP_LESS, bool colorWrite = true, bool depthWrite = true);
    /// Get a shader program and cache for later use.
    ShaderProgram* GetShaderProgram(unsigned char programBits);

    /// Return parent material.
    Material* Parent() const { return parent; }
    /// Return shader.
    Shader* GetShader() const { return shader; }
    /// Return blend mode.
    BlendMode GetBlendMode() const { return blendMode; }
    /// Return depth test mode.
    CompareMode GetDepthTest() const { return depthTest; }
    /// Return color write flag.
    bool GetColorWrite() const { return colorWrite; }
    /// Return depth write flag.
    bool GetDepthWrite() const { return depthWrite; }

    /// Last sort key for combined distance and state sorting. Used by Renderer.
    std::pair<unsigned short, unsigned short> lastSortKey;

private:
    /// Parent material.
    Material* parent;
    /// Blend mode.
    BlendMode blendMode;
    /// Depth test mode.
    CompareMode depthTest;
    /// Color write flag.
    bool colorWrite;
    /// Depth write flag.
    bool depthWrite;
    /// Cached shader variations.
    SharedPtr<ShaderProgram> shaderPrograms[MAX_SHADER_VARIATIONS];
    /// Shader resource.
    SharedPtr<Shader> shader;
    /// Vertex shader defines.
    std::string vsDefines;
    /// Fragment shader defines.
    std::string fsDefines;
};

/// %Material resource, which describes how to render 3D geometry and refers to textures. A material can contain several passes (for example normal rendering, and depth only.)
class Material : public Resource
{
    OBJECT(Material);

public:
    /// Construct.
    Material();
    /// Destruct.
    ~Material();

    /// Register object factory.
    static void RegisterObject();

    /// Load material from a stream. Return true on success.
    bool BeginLoad(Stream& source) override;
    /// Finalize material loading in the main thread. Return true on success.
    bool EndLoad() override;

    /// Create and return a new pass. If pass with same name exists, it will be returned.
    Pass* CreatePass(PassType type);
    /// Remove a pass.
    void RemovePass(PassType type);
    /// Set a texture.
    void SetTexture(size_t index, Texture* texture);
    /// Reset all texture assignments.
    void ResetTextures();
    /// Set shader defines for all passes.
    void SetShaderDefines(const std::string& vsDefines, const std::string& fsDefines);
    /// Set an uniform value. For simplicity and performance, all material uniforms are Vector4's.
    void SetUniform(PresetUniform uniform, const Vector4& value);
    /// Set culling mode, shared by all passes.
    void SetCullMode(CullMode mode);

    /// Return pass by index or null if not found.
    Pass* GetPass(PassType type) const { return passes[type]; }
    /// Return texture by texture unit.
    Texture* GetTexture(size_t index) const { return textures[index]; }
    /// Return vertex shader defines.
    const std::string& VSDefines() const { return vsDefines; }
    /// Return fragment shader defines.
    const std::string& FSDefines() const { return fsDefines; }
    /// Return uniform values.
    const std::map<PresetUniform, Vector4>& UniformValues() const { return uniformValues; }
    /// Return culling mode.
    CullMode GetCullMode() const { return cullMode; }

    /// Set global (lighting-related) shader defines. Resets all loaded pass shaders.
    static void SetGlobalShaderDefines(const std::string& vsDefines, const std::string& fsDefines);
    /// Return a default opaque untextured material.
    static Material* DefaultMaterial();
    /// Return global vertex shader defines.
    static const std::string& GlobalVSDefines() { return globalVSDefines; }
    /// Return global fragment shader defines.
    static const std::string& GlobalFSDefines() { return globalFSDefines; }

private:
    /// Culling mode.
    CullMode cullMode;
    /// Passes.
    SharedPtr<Pass> passes[MAX_PASS_TYPES];
    /// Material textures.
    SharedPtr<Texture> textures[MAX_MATERIAL_TEXTURE_UNITS];
    /// Uniform values.
    std::map<PresetUniform, Vector4> uniformValues;
    /// Vertex shader defines for all passes.
    std::string vsDefines;
    /// Fragment shader defines for all passes.
    std::string fsDefines;
    /// JSON data used for loading.
    AutoPtr<JSONFile> loadJSON;

    /// Default material.
    static SharedPtr<Material> defaultMaterial;
    /// All materials.
    static std::set<Material*> allMaterials;
    /// Global vertex shader defines.
    static std::string globalVSDefines;
    /// Global fragment shader defines.
    static std::string globalFSDefines;
};

extern const char* geometryDefines[];
extern const char* lightDefines[];
extern const char* dirLightDefines[];

inline ShaderProgram* Pass::GetShaderProgram(unsigned char programBits)
{
    if (shaderPrograms[programBits])
        return shaderPrograms[programBits];
    else
    {
        if (!shader)
            return nullptr;

        unsigned char geomBits = programBits & SP_GEOMETRYBITS;

        ShaderProgram* newShaderProgram = shader->CreateProgram(
            Material::GlobalVSDefines() + parent->VSDefines() + vsDefines + geometryDefines[geomBits],
            Material::GlobalFSDefines() + parent->FSDefines() + fsDefines
        );

        shaderPrograms[programBits] = newShaderProgram;
        return newShaderProgram;
    }
}
