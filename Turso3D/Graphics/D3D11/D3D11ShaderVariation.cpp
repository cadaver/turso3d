// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../Shader.h"
#include "D3D11Graphics.h"
#include "D3D11ShaderVariation.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ShaderVariation::ShaderVariation(Shader* parent_, const String& defines_) :
    parent(parent_),
    stage(parent->Stage()),
    defines(defines_),
    blob(0),
    shader(0),
    compiled(false)
{
}

ShaderVariation::~ShaderVariation()
{
    Release();
}

void ShaderVariation::Release()
{
    if (graphics && (graphics->CurrentVertexShader() == this || graphics->CurrentPixelShader() == this))
        graphics->SetShaders(0, 0);

    if (blob)
    {
        ID3DBlob* d3dBlob = (ID3DBlob*)blob;
        d3dBlob->Release();
        blob = 0;
    }

    if (shader)
    {
        if (stage == SHADER_VS)
        {
            ID3D11VertexShader* d3dShader = (ID3D11VertexShader*)shader;
            d3dShader->Release();
        }
        else
        {
            ID3D11PixelShader* d3dShader = (ID3D11PixelShader*)shader;
            d3dShader->Release();
        }
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
    Vector<D3D_SHADER_MACRO> macros;

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
    for (size_t i = 0; i < defineNames.Size(); ++i)
    {
        D3D_SHADER_MACRO macro;
        macro.Name = defineNames[i].CString();
        macro.Definition = defineValues[i].CString();
        macros.Push(macro);
    }
    D3D_SHADER_MACRO endMacro;
    endMacro.Name = 0;
    endMacro.Definition = 0;
    macros.Push(endMacro);

    DWORD flags = 0;
    ID3DBlob* errorBlob = 0;
    if (FAILED(D3DCompile(parent->SourceCode().CString(), parent->SourceCode().Length(), "", &macros[0], 0, "main", 
        stage == SHADER_VS ? "vs_4_0" : "ps_4_0", flags, 0, (ID3DBlob**)&blob, &errorBlob)))
    {
        if (errorBlob)
        {
            String errorString((const char*)errorBlob->GetBufferPointer());
            LOGERROR("Failed to compile shader " + FullName() + ": " + errorString);
            errorBlob->Release();
        }
        return false;
    }

    if (errorBlob)
    {
        errorBlob->Release();
        errorBlob = 0;
    }
    
    ID3D11Device* d3dDevice = (ID3D11Device*)graphics->Device();
    ID3DBlob* d3dBlob = (ID3DBlob*)blob;

    if (stage == SHADER_VS)
        d3dDevice->CreateVertexShader(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), 0, (ID3D11VertexShader**)&shader);
    else
        d3dDevice->CreatePixelShader(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), 0, (ID3D11PixelShader**)&shader);

    if (!shader)
    {
        LOGERROR("Failed to create shader " + FullName());
        return false;
    }
    else
        LOGDEBUGF("Compiled shader %s bytecode size %d", FullName().CString(), d3dBlob->GetBufferSize());
    
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