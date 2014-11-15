// For conditions of distribution and use, see copyright notice in License.txt

#include "../Debug/Log.h"
#include "../Debug/Profiler.h"
#include "../Resource/ResourceCache.h"
#include "../IO/File.h"
#include "../IO/FileSystem.h"
#include "Shader.h"
#include "ShaderVariation.h"

namespace Turso3D
{

Shader::Shader() :
    stage(SHADER_VS)
{
}

Shader::~Shader()
{
}

void Shader::RegisterObject()
{
    RegisterFactory<Shader>();
}

bool Shader::BeginLoad(Stream& source)
{
    stage = Extension(source.Name()) == ".ps" ? SHADER_PS : SHADER_VS;
    sourceCode.Clear();
    return ProcessIncludes(sourceCode, source);
}

bool Shader::EndLoad()
{
    // Release existing variations (if any) to allow them to be recompiled with changed code
    for (auto it = variations.Begin(); it != variations.End(); ++it)
        it->second->Release();
    return true;
}

void Shader::Define(ShaderStage stage_, const String& code)
{
    stage = stage_;
    sourceCode = code;
    EndLoad();
}

ShaderVariation* Shader::CreateVariation(const String& definesIn)
{
    StringHash definesHash(definesIn);
    auto it = variations.Find(definesHash);
    if (it != variations.End())
        return it->second.Get();
    
    // If initially not found, normalize the defines and try again
    String defines = NormalizeDefines(definesIn);
    definesHash = defines;
    it = variations.Find(definesHash);
    if (it != variations.End())
        return it->second.Get();

    ShaderVariation* newVariation = new ShaderVariation(this, defines);
    variations[definesHash] = newVariation;
    return newVariation;
}

bool Shader::ProcessIncludes(String& code, Stream& source)
{
    ResourceCache* cache = Subsystem<ResourceCache>();

    while (!source.IsEof())
    {
        String line = source.ReadLine();

        if (line.StartsWith("#include"))
        {
            String includeFileName = Path(source.Name()) + line.Substring(9).Replaced("\"", "").Trimmed();
            File includeFile;
            if (!cache->OpenFile(includeFile, includeFileName))
                return false;

            // Add the include file into the current code recursively
            if (!ProcessIncludes(code, includeFile))
                return false;
        }
        else
        {
            code += line;
            code += "\n";
        }
    }

    // Finally insert an empty line to mark the space between files
    code += "\n";

    return true;
}

String Shader::NormalizeDefines(const String& defines)
{
    String ret;
    Vector<String> definesVec = defines.ToUpper().Split(' ');
    Sort(definesVec.Begin(), definesVec.End());

    for (auto it = definesVec.Begin(); it != definesVec.End(); ++it)
    {
        if (it != definesVec.Begin())
            ret += " ";
        ret += *it;
    }

    return ret;
}

}
