// For conditions of distribution and use, see copyright notice in License.txt

#include "../Graphics/Texture.h"
#include "../Graphics/UniformBuffer.h"
#include "../IO/StringUtils.h"
#include "../Resource/JSONFile.h"
#include "../Resource/ResourceCache.h"
#include "Material.h"

#include <tracy/Tracy.hpp>

const char* passNames[] = 
{
    "shadow",
    "opaque",
    "alpha",
    nullptr
};

const char* geometryDefines[] = 
{
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
    blendMode(BLEND_REPLACE),
    depthTest(CMP_LESS_EQUAL),
    colorWrite(true),
    depthWrite(true)
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
        (CompareMode)ListIndex(source["depthTest"].GetString(), compareModeNames, CMP_LESS_EQUAL),
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
    cullMode(CULL_BACK),
    uniformsDirty(false)
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

    if (root.Contains("uniforms"))
    {
        std::vector<std::pair<std::string, Vector4> > newUniforms;

        const JSONArray& jsonUniforms = root["uniforms"].GetArray();
        for (auto it = jsonUniforms.begin(); it != jsonUniforms.end(); ++it)
        {
            const JSONObject& jsonUniform = it->GetObject();
            if (jsonUniform.size() == 1)
            {
                auto uIt = jsonUniform.begin();
                newUniforms.push_back(std::make_pair(uIt->first, Vector4(uIt->second.GetString())));
            }
        }

        DefineUniforms(newUniforms);
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

SharedPtr<Material> Material::Clone()
{
    SharedPtr<Material> ret(Object::Create<Material>());
    
    ret->cullMode = cullMode;

    for (size_t i = 0; i < MAX_PASS_TYPES; ++i)
    {
        Pass* pass = passes[i];

        if (pass)
        {
            Pass* clonePass = ret->CreatePass((PassType)i);
            clonePass->SetShader(pass->GetShader(), pass->VSDefines(), pass->FSDefines());
            clonePass->SetRenderState(pass->GetBlendMode(), pass->GetDepthTest(), pass->GetColorWrite(), pass->GetDepthWrite());
        }
    }

    for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i)
        ret->textures[i] = textures[i];

    ret->uniformBuffer = uniformBuffer;
    ret->uniformValues = uniformValues;
    ret->uniformNameHashes = uniformNameHashes;
    ret->vsDefines = vsDefines;
    ret->fsDefines = fsDefines;

    return ret;
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

void Material::DefineUniforms(size_t numUniforms, const char** uniformNames)
{
    uniformNameHashes.resize(numUniforms);
    uniformValues.resize(numUniforms);

    for (size_t i = 0; i < numUniforms; ++i)
        uniformNameHashes[i] = StringHash(uniformNames[i]);

    uniformsDirty = true;
}

void Material::DefineUniforms(const std::vector<std::string>& uniformNames)
{
    uniformNameHashes.resize(uniformNames.size());
    uniformValues.resize(uniformNames.size());

    for (size_t i = 0; i < uniformNames.size(); ++i)
        uniformNameHashes[i] = StringHash(uniformNames[i]);

    uniformsDirty = true;
}

void Material::DefineUniforms(const std::vector<std::pair<std::string, Vector4> >& uniforms)
{
    uniformValues.resize(uniforms.size());
    uniformNameHashes.resize(uniforms.size());

    for (size_t i = 0; i < uniforms.size(); ++i)
    {
        uniformNameHashes[i] = StringHash(uniforms[i].first);
        uniformValues[i] = uniforms[i].second;
    }

    uniformsDirty = true;
}

void Material::SetUniform(size_t index, const Vector4& value)
{
    if (index >= uniformValues.size())
        return;

    uniformValues[index] = value;
    uniformsDirty = true;
}

void Material::SetUniform(const std::string& name_, const Vector4& value)
{
    SetUniform(StringHash(name_), value);
}

void Material::SetUniform(const char* name_, const Vector4& value)
{
    SetUniform(StringHash(name_), value);
}

void Material::SetUniform(StringHash nameHash_, const Vector4& value)
{
    for (size_t i = 0; i < uniformNameHashes.size(); ++i)
    {
        if (uniformNameHashes[i] == nameHash_)
        {
            uniformValues[i] = value;
            uniformsDirty = true;
            return;
        }
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

void Material::SetCullMode(CullMode mode)
{
    cullMode = mode;
}

UniformBuffer* Material::GetUniformBuffer() const
{
    if (uniformsDirty)
    {
        // If is a shared uniform buffer from clone operation, make unique now
        if (!uniformBuffer || uniformBuffer->Refs() > 1)
            uniformBuffer = new UniformBuffer();

        if (uniformValues.size())
        {
            if (uniformBuffer->Size() != uniformValues.size() * sizeof(Vector4))
                uniformBuffer->Define(USAGE_DEFAULT, uniformValues.size() * sizeof(Vector4), &uniformValues[0]);
            else
                uniformBuffer->SetData(0, uniformValues.size() * sizeof(Vector4), &uniformValues[0]);
        }

        uniformsDirty = false;
    }

    return uniformBuffer;
}

const Vector4& Material::Uniform(const std::string& name_) const
{
    return Uniform(StringHash(name_));
}

const Vector4& Material::Uniform(const char* name_) const
{
    return Uniform(StringHash(name_));
}

const Vector4& Material::Uniform(StringHash nameHash_) const
{
    for (size_t i = 0; i < uniformNameHashes.size(); ++i)
    {
        if (uniformNameHashes[i] == nameHash_)
            return uniformValues[i];
    }

    return Vector4::ZERO;
}

Material* Material::DefaultMaterial()
{
    ResourceCache* cache = Subsystem<ResourceCache>();

    // Create on demand
    if (!defaultMaterial)
    {
        defaultMaterial = Create<Material>();

        std::vector<std::pair<std::string, Vector4> > defaultUniforms;
        defaultUniforms.push_back(std::make_pair("matDiffColor", Vector4::ONE));
        defaultUniforms.push_back(std::make_pair("matSpecColor", Vector4(0.25f, 0.25f, 0.25f, 1.0f)));
        defaultMaterial->DefineUniforms(defaultUniforms);

        Pass* pass = defaultMaterial->CreatePass(PASS_SHADOW);
        pass->SetShader(cache->LoadResource<Shader>("Shaders/Shadow.glsl"), "", "");
        pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, false, true);

        pass = defaultMaterial->CreatePass(PASS_OPAQUE);
        pass->SetShader(cache->LoadResource<Shader>("Shaders/NoTexture.glsl"), "", "");
        pass->SetRenderState(BLEND_REPLACE, CMP_LESS_EQUAL, true, true);
    }

    return defaultMaterial;
}
