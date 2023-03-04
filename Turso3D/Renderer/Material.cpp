// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/Texture.h"
#include "../IO/StringUtils.h"
#include "../Resource/JSONFile.h"
#include "../Resource/ResourceCache.h"
#include "Material.h"

#include <tracy/Tracy.hpp>

const char* passNames[] = {
    "shadow",
    "opaque",
    "opaqueadd",
    "alpha",
    "alphaadd",
    nullptr
};

const char* geometryDefines[] = {
    "",
    "SKINNED ",
    "INSTANCED ",
    "",
    nullptr
};

std::set<Material*> Material::allMaterials;
SharedPtr<Material> Material::defaultMaterial;
std::string Material::globalVSDefines;
std::string Material::globalFSDefines;

Pass::Pass(Material* parent_) :
    parent(parent_),
    colorWrite(true),
    depthWrite(true),
    depthTest(CMP_LESS),
    blendMode(BLEND_REPLACE)
{
}

Pass::~Pass()
{
}

void Pass::LoadJSON(const JSONValue& source)
{
    ZoneScoped;

    ResourceCache* cache = Object::Subsystem<ResourceCache>();

    SetShader(cache->LoadResource<Shader>(source["shader"].GetString()), source["vsDefines"].GetString(), source["fsDefines"].GetString());

    SetRenderState(
        (BlendMode)ListIndex(source["blendMode"].GetString(), blendModeNames, BLEND_REPLACE), 
        (CompareMode)ListIndex(source["depthTest"].GetString(), compareModeNames, CMP_LESS),
        source.Contains("colorWrite") ? source["colorWrite"].GetBool() : true, 
        source.Contains("depthWrite") ? source["depthWrite"].GetBool() : true
    );
}

void Pass::SetShader(Shader* shader_, const std::string& vsDefines_, const std::string& fsDefines_)
{
    shader = shader_;
    vsDefines = vsDefines_;
    fsDefines = fsDefines_;
    if (vsDefines.length())
        vsDefines += " ";
    if (fsDefines.length())
        fsDefines += " ";

    ResetShaderPrograms();
}

void Pass::SetRenderState(BlendMode blendMode_, CompareMode depthTest_, bool colorWrite_, bool depthWrite_)
{
    blendMode = blendMode_;
    depthTest = depthTest_;
    colorWrite = colorWrite_;
    depthWrite = depthWrite_;
}

void Pass::ResetShaderPrograms()
{
    for (size_t i = 0; i < MAX_SHADER_VARIATIONS; ++i)
        shaderPrograms[i].Reset();
}

Material::Material() :
    cullMode(CULL_BACK)
{
    allMaterials.insert(this);
}

Material::~Material()
{
    allMaterials.erase(this);
}

void Material::RegisterObject()
{
    RegisterFactory<Material>();
}

bool Material::BeginLoad(Stream& source)
{
    ZoneScoped;

    loadJSON = new JSONFile();
    if (!loadJSON->Load(source))
        return false;

    const JSONValue& root = loadJSON->Root();

    uniformValues.clear();
    if (root.Contains("uniformValues"))
    {
        const JSONObject& jsonUniformValues = root["uniformValues"].GetObject();
        for (auto it = jsonUniformValues.begin(); it != jsonUniformValues.end(); ++it)
        {
            PresetUniform uniform = (PresetUniform)ListIndex(it->first.c_str(), presetUniformNames, MAX_PRESET_UNIFORMS);
            if (uniform != MAX_PRESET_UNIFORMS)
                uniformValues[uniform] = Vector4(it->second.GetString());
        }
    }

    if (root.Contains("cullMode"))
        cullMode = (CullMode)ListIndex(root["cullMode"].GetString(), cullModeNames, CULL_BACK);
    else
        cullMode = CULL_BACK;

    return true;
}

bool Material::EndLoad()
{
    ZoneScoped;

    const JSONValue& root = loadJSON->Root();

    for (size_t i = 0; i < MAX_PASS_TYPES; ++i)
        passes[i].Reset();

    SetShaderDefines(root["vsDefines"].GetString(), root["fsDefines"].GetString());

    if (root.Contains("passes"))
    {
        const JSONObject& jsonPasses = root["passes"].GetObject();
        for (auto it = jsonPasses.begin(); it != jsonPasses.end(); ++it)
        {
            PassType type = (PassType)ListIndex(it->first.c_str(), passNames, MAX_PASS_TYPES);
            if (type != MAX_PASS_TYPES)
            {
                Pass* newPass = CreatePass(type);
                newPass->LoadJSON(it->second);
            }
        }
    }
    
    /// \todo Queue texture loads during BeginLoad()
    ResetTextures();
    if (root.Contains("textures"))
    {
        ResourceCache* cache = Subsystem<ResourceCache>();
        const JSONObject& jsonTextures = root["textures"].GetObject();
        for (auto it = jsonTextures.begin(); it != jsonTextures.end(); ++it)
            SetTexture(ParseInt(it->first), cache->LoadResource<Texture>(it->second.GetString()));
    }

    loadJSON.Reset();
    return true;
}

Pass* Material::CreatePass(PassType type)
{
    if (!passes[type])
        passes[type] = new Pass(this);
    
    return passes[type];
}

void Material::RemovePass(PassType type)
{
    passes[type].Reset();
}

void Material::SetTexture(size_t index, Texture* texture)
{
    if (index < MAX_MATERIAL_TEXTURE_UNITS)
        textures[index] = texture;
}

void Material::ResetTextures()
{
    for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i)
        textures[i].Reset();
}

void Material::SetShaderDefines(const std::string& vsDefines_, const std::string& fsDefines_)
{
    vsDefines = vsDefines_;
    fsDefines = fsDefines_;
    if (vsDefines.length())
        vsDefines += " ";
    if (fsDefines.length())
        fsDefines += " ";
    
    for (size_t i = 0; i < MAX_PASS_TYPES; ++i)
    {
        if (passes[i])
            passes[i]->ResetShaderPrograms();
    }
}

void Material::SetGlobalShaderDefines(const std::string& vsDefines_, const std::string& fsDefines_)
{
    globalVSDefines = vsDefines_;
    globalFSDefines = fsDefines_;
    if (globalVSDefines.length())
        globalVSDefines += " ";
    if (globalFSDefines.length())
        globalFSDefines += " ";

    for (auto it = allMaterials.begin(); it != allMaterials.end(); ++it)
    {
        Material* material = *it;

        for (size_t i = 0; i < MAX_PASS_TYPES; ++i)
        {
            if (material->passes[i])
                material->passes[i]->ResetShaderPrograms();
        }
    }
}

void Material::SetUniform(PresetUniform uniform, const Vector4& value)
{
    uniformValues[uniform] = value;
}

void Material::SetCullMode(CullMode mode)
{
    cullMode = mode;
}

Material* Material::DefaultMaterial()
{
    ResourceCache* cache = Subsystem<ResourceCache>();

    // Create on demand
    if (!defaultMaterial)
    {
        defaultMaterial = new Material();
        defaultMaterial->SetUniform(U_MATDIFFCOLOR, Vector4::ONE);

        Pass* pass = defaultMaterial->CreatePass(PASS_SHADOW);
        pass->SetShader(cache->LoadResource<Shader>("Shaders/Shadow.glsl"), "", "");
        pass->SetRenderState(BLEND_REPLACE, CMP_LESS, false, true);

        pass = defaultMaterial->CreatePass(PASS_OPAQUE);
        pass->SetShader(cache->LoadResource<Shader>("Shaders/NoTexture.glsl"), "", "");
        pass->SetRenderState(BLEND_REPLACE, CMP_LESS, true, true);
    }

    return defaultMaterial.Get();
}
