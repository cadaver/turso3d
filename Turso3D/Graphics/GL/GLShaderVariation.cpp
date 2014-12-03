// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../Shader.h"
#include "GLGraphics.h"
#include "GLShaderVariation.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ShaderVariation::ShaderVariation(Shader* parent_, const String& defines_) :
    shader(0),
    parent(parent_),
    stage(parent->Stage()),
    defines(defines_),
    compiled(false)
{
}

ShaderVariation::~ShaderVariation()
{
    Release();
}

void ShaderVariation::Release()
{
    if (graphics)
    {
        if (graphics->GetVertexShader() == this || graphics->GetPixelShader() == this)
            graphics->SetShaders(nullptr, nullptr);
        graphics->CleanupShaderPrograms(this);
    }

    if (shader)
    {
        glDeleteShader(shader);
        shader = 0;
    }

    compiled = false;
}

bool ShaderVariation::Compile()
{
    if (compiled)
        return shader != 0;

    PROFILE(CompileShaderVariation);

    // Do not retry without a Release() inbetween
    compiled = true;

    if (!graphics || !graphics->IsInitialized())
    {
        LOGERROR("Can not compile shader without initialized Graphics subsystem");
        return false;
    }
    if (!parent)
    {
        LOGERROR("Can not compile shader without parent shader resource");
        return false;
    }

    shader = glCreateShader(stage == SHADER_VS ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
    if (!shader)
    {
        LOGERROR("Could not create shader object");
        return false;
    }

    // Collect defines into macros
    Vector<String> defineNames = defines.Split(' ');
    Vector<String> defineValues;

    for (size_t i = 0; i < defineNames.Size(); ++i)
    {
        size_t equalsPos = defineNames[i].Find('=');
        if (equalsPos != String::NPOS)
        {
            defineValues.Push(defineNames[i].Substring(equalsPos + 1));
            defineNames[i].Resize(equalsPos);
        }
        else
            defineValues.Push("1");
    }

    const String& originalShaderCode = parent->SourceCode();
    String shaderCode;

    // Check if the shader code contains a version define
    size_t verStart = originalShaderCode.Find('#');
    size_t verEnd = 0;
    if (verStart != String::NPOS)
    {
        if (originalShaderCode.Substring(verStart + 1, 7) == "version")
        {
            verEnd = verStart + 9;
            while (verEnd < originalShaderCode.Length())
            {
                if (IsDigit(originalShaderCode[verEnd]))
                    ++verEnd;
                else
                    break;
            }
            // If version define found, insert it first
            String versionDefine = originalShaderCode.Substring(verStart, verEnd - verStart);
            shaderCode += versionDefine + "\n";
        }
    }

    // Prepend the defines to the shader code
    for (unsigned i = 0; i < defineNames.Size(); ++i)
    {
        String defineString = "#define " + defineNames[i] + " " + defineValues[i];
        shaderCode += defineString;
    }

    // When version define found, do not insert it a second time
    if (verEnd > 0)
        shaderCode += (originalShaderCode.CString() + verEnd);
    else
        shaderCode += originalShaderCode;

    const char* shaderCStr = shaderCode.CString();
    glShaderSource(shader, 1, &shaderCStr, 0);
    glCompileShader(shader);

    int compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        int length, outLength;
        String errorString;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        errorString.Resize(length);
        glGetShaderInfoLog(shader, length, &outLength, &errorString[0]);
        glDeleteShader(shader);
        shader = 0;

        LOGERRORF("Could not compile shader %s: %s", FullName().CString(), errorString.CString());
        return false;
    }

    LOGDEBUG("Compiled shader " + FullName());
    return true;
}

Shader* ShaderVariation::Parent() const
{
    return parent;
}

String ShaderVariation::FullName() const
{
    if (parent)
        return defines.IsEmpty() ? parent->Name() : parent->Name() + " (" + defines + ")";
    else
        return String::EMPTY;
}

}