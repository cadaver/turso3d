// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../Shader.h"
#include "GLGraphics.h"
#include "GLShaderVariation.h"
#include "GLVertexBuffer.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ShaderVariation::ShaderVariation(Shader* parent_, const String& defines_) :
    parent(parent_),
    stage(parent->Stage()),
    defines(defines_),
    elementHash(0),
    compiled(false)
{
}

ShaderVariation::~ShaderVariation()
{
    Release();
}

void ShaderVariation::Release()
{
    if (graphics && (graphics->GetVertexShader() == this || graphics->GetPixelShader() == this))
        graphics->SetShaders(nullptr, nullptr);

    /// \todo Destroy OpenGL shader
    elementHash = 0;
    compiled = false;
}

bool ShaderVariation::Compile()
{
    if (compiled)
        return true;

    PROFILE(CompileShaderVariation);

    // Do not retry without a Release() inbetween
    compiled = true;

    if (!parent)
    {
        LOGERROR("Can not compile shader without parent shader resource");
        return false;
    }
    if (!graphics || !graphics->IsInitialized())
    {
        LOGERROR("Can not compile shader without initialized Graphics subsystem");
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

    /// \todo Compile OpenGL shader

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