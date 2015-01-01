// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Profiler.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/ShaderVariation.h"
#include "../Graphics/Shader.h"
#include "../Graphics/Texture.h"
#include "../Resource/JSONFile.h"
#include "../Resource/ResourceCache.h"
#include "Material.h"

#include "../Debug/DebugNew.h"

namespace Turso3D
{

SharedPtr<Material> Material::defaultMaterial;
HashMap<String, unsigned char> Material::passIndices;
Vector<String> Material::passNames;
unsigned char Material::nextPassIndex = 0;

Pass::Pass(Material* parent_, const String& name_) :
    parent(parent_),
    name(name_),
    shadersLoaded(false)
{
    Reset();
}

Pass::~Pass()
{
}

bool Pass::LoadJSON(const JSONValue& source)
{
    if (source.Contains("vs"))
        shaderNames[SHADER_VS] = source["vs"].GetString();
    if (source.Contains("ps"))
        shaderNames[SHADER_PS] = source["ps"].GetString();
    if (source.Contains("vsDefines"))
        shaderDefines[SHADER_VS] = source["vsDefines"].GetString();
    if (source.Contains("psDefines"))
        shaderDefines[SHADER_PS] = source["psDefines"].GetString();

    if (source.Contains("depthFunc"))
        depthFunc = (CompareFunc)String::ListIndex(source["depthFunc"].GetString(), compareFuncNames, CMP_LESS_EQUAL);
    if (source.Contains("depthWrite"))
        depthWrite = source["depthWrite"].GetBool();
    if (source.Contains("depthClip"))
        depthClip = source["depthClip"].GetBool();
    if (source.Contains("alphaToCoverage"))
        alphaToCoverage = source["alphaToCoverage"].GetBool();
    if (source.Contains("blendMode"))
        blendMode = blendModes[String::ListIndex(source["blendMode"].GetString(), blendModeNames, BLEND_MODE_REPLACE)];
    else
    {
        if (source.Contains("blendEnable"))
            blendMode.blendEnable = source["blendEnable"].GetBool();
        if (source.Contains("srcBlend"))
            blendMode.srcBlend = (BlendFactor)String::ListIndex(source["srcBlend"].GetString(), blendFactorNames, BLEND_ONE);
        if (source.Contains("destBlend"))
            blendMode.destBlend = (BlendFactor)String::ListIndex(source["destBlend"].GetString(), blendFactorNames, BLEND_ONE);
        if (source.Contains("blendOp"))
            blendMode.blendOp = (BlendOp)String::ListIndex(source["blendOp"].GetString(), blendOpNames, BLEND_OP_ADD);
        if (source.Contains("srcBlendAlpha"))
            blendMode.srcBlendAlpha = (BlendFactor)String::ListIndex(source["srcBlendAlpha"].GetString(), blendFactorNames, BLEND_ONE);
        if (source.Contains("destBlendAlpha"))
            blendMode.destBlendAlpha = (BlendFactor)String::ListIndex(source["destBlendAlpha"].GetString(), blendFactorNames, BLEND_ONE);
        if (source.Contains("blendOpAlpha"))
            blendMode.blendOpAlpha = (BlendOp)String::ListIndex(source["blendOpAlpha"].GetString(), blendOpNames, BLEND_OP_ADD);
    }

    if (source.Contains("fillMode"))
        fillMode = (FillMode)String::ListIndex(source["fillMode"].GetString(), fillModeNames, FILL_SOLID);
    if (source.Contains("cullMode"))
        cullMode = (CullMode)String::ListIndex(source["cullMode"].GetString(), cullModeNames, CULL_BACK);

    return true;
}

bool Pass::SaveJSON(JSONValue& dest)
{
    dest.SetEmptyObject();

    if (shaderNames[SHADER_VS].Length())
        dest["vs"] = shaderNames[SHADER_VS];
    if (shaderNames[SHADER_PS].Length())
        dest["ps"] = shaderNames[SHADER_PS];
    if (shaderDefines[SHADER_VS].Length())
        dest["vsDefines"] = shaderDefines[SHADER_VS];
    if (shaderDefines[SHADER_PS].Length())
        dest["psDefines"] = shaderDefines[SHADER_PS];

    dest["depthFunc"] = compareFuncNames[depthFunc];
    dest["depthWrite"] = depthWrite;
    dest["depthClip"] = depthClip;
    dest["alphaToCoverage"] = alphaToCoverage;

    // Prefer saving a predefined blend mode if possible for better readability
    bool blendModeFound = false;
    for (size_t i = 0; i < MAX_BLEND_MODES; ++i)
    {
        if (blendMode == blendModes[i])
        {
            dest["blendMode"] = blendModeNames[i];
            blendModeFound = true;
            break;
        }
    }

    if (!blendModeFound)
    {
        dest["blendEnable"] = blendMode.blendEnable;
        dest["srcBlend"] = blendFactorNames[blendMode.srcBlend];
        dest["destBlend"] = blendFactorNames[blendMode.destBlend];
        dest["blendOp"] = blendOpNames[blendMode.blendOp];
        dest["srcBlendAlpha"] = blendFactorNames[blendMode.srcBlendAlpha];
        dest["destBlendAlpha"] = blendFactorNames[blendMode.destBlendAlpha];
        dest["blendOpAlpha"] = blendOpNames[blendMode.blendOpAlpha];
    }

    dest["fillMode"] = fillModeNames[fillMode];
    dest["cullMode"] = cullModeNames[cullMode];

    return true;
}

void Pass::SetBlendMode(BlendMode mode)
{
    blendMode = blendModes[mode];
}

void Pass::SetShaders(const String& vsName, const String& psName, const String& vsDefines, const String& psDefines)
{
    shaderNames[SHADER_VS] = vsName;
    shaderNames[SHADER_PS] = psName;
    shaderDefines[SHADER_VS] = vsDefines;
    shaderDefines[SHADER_PS] = psDefines;
    ClearCachedShaders();
}

void Pass::Reset()
{
    depthFunc = CMP_LESS_EQUAL;
    depthWrite = true;
    depthClip = true;
    alphaToCoverage = false;
    blendMode.Reset();
    cullMode = CULL_BACK;
    fillMode = FILL_SOLID;
}

void Pass::ClearCachedShaders()
{
    for (size_t i = 0; i < MAX_SHADER_STAGES; ++i)
    {
        shaders[i].Reset();
        shaderVariations[i].Clear();
    }

    shadersLoaded = false;
}

Material* Pass::Parent() const
{
    return parent;
}

Material::Material()
{
}

Material::~Material()
{
}

void Material::RegisterObject()
{
    RegisterFactory<Material>();
}

bool Material::BeginLoad(Stream& source)
{
    PROFILE(BeginLoadMaterial);

    loadJSON = new JSONFile();
    if (!loadJSON->Load(source))
        return false;

    const JSONValue& root = loadJSON->Root();

    shaderDefines[SHADER_VS].Clear();
    shaderDefines[SHADER_PS].Clear();
    if (root.Contains("vsDefines"))
        shaderDefines[SHADER_VS] = root["vsDefines"].GetString();
    if (root.Contains("psDefines"))
        shaderDefines[SHADER_PS] = root["psDefines"].GetString();

    return true;
}

bool Material::EndLoad()
{
    PROFILE(EndLoadMaterial);

    const JSONValue& root = loadJSON->Root();

    passes.Clear();
    if (root.Contains("passes"))
    {
        const JSONObject& jsonPasses = root["passes"].GetObject();
        for (auto it = jsonPasses.Begin(); it != jsonPasses.End(); ++it)
        {
            Pass* newPass = CreatePass(it->first);
            newPass->LoadJSON(it->second);
        }
    }

    constantBuffers[SHADER_VS].Reset();
    if (root.Contains("vsConstantBuffer"))
    {
        constantBuffers[SHADER_VS] = new ConstantBuffer();
        constantBuffers[SHADER_VS]->LoadJSON(root["vsConstantBuffer"].GetObject());
    }

    constantBuffers[SHADER_PS].Reset();
    if (root.Contains("psConstantBuffer"))
    {
        constantBuffers[SHADER_PS] = new ConstantBuffer();
        constantBuffers[SHADER_PS]->LoadJSON(root["psConstantBuffer"].GetObject());
    }
    
    /// \todo Queue texture loads during BeginLoad()
    ResetTextures();
    if (root.Contains("textures"))
    {
        ResourceCache* cache = Subsystem<ResourceCache>();
        const JSONObject& jsonTextures = root["textures"].GetObject();
        for (auto it = jsonTextures.Begin(); it != jsonTextures.End(); ++it)
            SetTexture(it->first.ToInt(), cache->LoadResource<Texture>(it->second.GetString()));
    }

    loadJSON.Reset();
    return true;
}

bool Material::Save(Stream& dest)
{
    PROFILE(SaveMaterial);

    JSONFile saveJSON;
    JSONValue& root = saveJSON.Root();
    root.SetEmptyObject();

    if (shaderDefines[SHADER_VS].Length())
        root["vsDefines"] = shaderDefines[SHADER_VS];
    if (shaderDefines[SHADER_PS].Length())
        root["psDefines"] = shaderDefines[SHADER_PS];
    
    if (passes.Size())
    {
        root["passes"].SetEmptyObject();
        for (auto it = passes.Begin(); it != passes.End(); ++it)
        {
            Pass* pass = *it;
            if (pass)
                pass->SaveJSON(root["passes"][pass->Name()]);
        }
    }

    if (constantBuffers[SHADER_VS])
        constantBuffers[SHADER_VS]->SaveJSON(root["vsConstantBuffer"]);
    if (constantBuffers[SHADER_PS])
        constantBuffers[SHADER_PS]->SaveJSON(root["psConstantBuffer"]);

    root["textures"].SetEmptyObject();
    for (size_t i = 0; i < MAX_MATERIAL_TEXTURE_UNITS; ++i)
    {
        if (textures[i])
            root["textures"][String((int)i)] = textures[i]->Name();
    }

    return saveJSON.Save(dest);
}

Pass* Material::CreatePass(const String& name)
{
    size_t index = PassIndex(name);
    if (passes.Size() <= index)
        passes.Resize(index + 1);

    if (!passes[index])
        passes[index] = new Pass(this, name);
    
    return passes[index];
}

void Material::RemovePass(const String& name)
{
    size_t index = PassIndex(name, false);
    if (index < passes.Size())
        passes[index].Reset();
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

void Material::SetConstantBuffer(ShaderStage stage, ConstantBuffer* buffer)
{
    if (stage < MAX_SHADER_STAGES)
        constantBuffers[stage] = buffer;
}

void Material::SetShaderDefines(ShaderStage stage, const String& defines)
{
    if (stage < MAX_SHADER_STAGES)
    {
        shaderDefines[stage] = defines;
        for (auto it = passes.Begin(); it != passes.End(); ++it)
        {
            Pass* pass = *it;
            if (pass)
                pass->ClearCachedShaders();
        }
    }
}

Pass* Material::FindPass(const String& name) const
{
    return GetPass(PassIndex(name, false));
}

Pass* Material::GetPass(unsigned char index) const
{
    return index < passes.Size() ? passes[index].Get() : nullptr;
}

Texture* Material::GetTexture(size_t index) const
{
    return index < MAX_MATERIAL_TEXTURE_UNITS ? textures[index].Get() : nullptr;
}

ConstantBuffer* Material::GetConstantBuffer(ShaderStage stage) const
{
    return stage < MAX_SHADER_STAGES ? constantBuffers[stage].Get() : nullptr;
}

const String& Material::ShaderDefines(ShaderStage stage) const
{
    return stage < MAX_SHADER_STAGES ? shaderDefines[stage] : String::EMPTY;
}

unsigned char Material::PassIndex(const String& name, bool createNew)
{
    String nameLower = name.ToLower();
    auto it = passIndices.Find(nameLower);
    if (it != passIndices.End())
        return it->second;
    else
    {
        if (createNew)
        {
            passIndices[nameLower] = nextPassIndex;
            passNames.Push(nameLower);
            return nextPassIndex++;
        }
        else
            return 0xff;
    }
}

const String& Material::PassName(unsigned char index)
{
    return index < passNames.Size() ? passNames[index] : String::EMPTY;
}

Material* Material::DefaultMaterial()
{
    // Create on demand
    if (!defaultMaterial)
    {
        defaultMaterial = new Material();
        Pass* pass = defaultMaterial->CreatePass("opaque");
        pass->SetShaders("NoTexture", "NoTexture");

        pass = defaultMaterial->CreatePass("opaqueadd");
        pass->SetShaders("NoTexture", "NoTexture");
        pass->SetBlendMode(BLEND_MODE_ADD);
        pass->depthWrite = false;
    }

    return defaultMaterial.Get();
}


}
