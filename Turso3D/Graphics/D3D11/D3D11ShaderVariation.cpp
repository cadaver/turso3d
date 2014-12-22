// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "../Shader.h"
#include "D3D11Graphics.h"
#include "D3D11ShaderVariation.h"
#include "D3D11VertexBuffer.h"

#include <d3d11.h>
#include <d3dcompiler.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

unsigned InspectInputSignature(ID3DBlob* d3dBlob)
{
    ID3D11ShaderReflection* reflection = nullptr;
    D3D11_SHADER_DESC shaderDesc;
    unsigned elementHash = 0;

    D3DReflect(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), IID_ID3D11ShaderReflection, (void**)&reflection);
    if (!reflection)
    {
        LOGERROR("Failed to reflect vertex shader's input signature");
        return elementHash;
    }

    reflection->GetDesc(&shaderDesc);
    for (size_t i = 0; i < shaderDesc.InputParameters; ++i)
    {
        D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
        reflection->GetInputParameterDesc((unsigned)i, &paramDesc);

        for (size_t j = 0; elementSemanticNames[j]; ++j)
        {
            if (!String::Compare(paramDesc.SemanticName, elementSemanticNames[j]))
            {
                elementHash |= VertexBuffer::ElementHash(i, (ElementSemantic)j);
                break;
            }
        }
    }

    reflection->Release();
    return elementHash;
}

ShaderVariation::ShaderVariation(Shader* parent_, const String& defines_) :
    parent(parent_),
    stage(parent->Stage()),
    defines(defines_),
    blob(nullptr),
    shader(nullptr),
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

    if (blob)
    {
        ID3DBlob* d3dBlob = (ID3DBlob*)blob;
        d3dBlob->Release();
        blob = nullptr;
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
        shader = nullptr;
    }

    elementHash = 0;
    compiled = false;
}

bool ShaderVariation::Compile()
{
    if (compiled)
        return shader != nullptr;

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
    endMacro.Name = nullptr;
    endMacro.Definition = nullptr;
    macros.Push(endMacro);

    /// \todo Optimization level
    DWORD flags = 0;
    ID3DBlob* errorBlob = nullptr;
    if (FAILED(D3DCompile(parent->SourceCode().CString(), parent->SourceCode().Length(), "", &macros[0], 0, "main", 
        stage == SHADER_VS ? "vs_4_0" : "ps_4_0", flags, 0, (ID3DBlob**)&blob, &errorBlob)))
    {
        if (errorBlob)
        {
            LOGERRORF("Could not compile shader %s: %s", FullName().CString(), errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    if (errorBlob)
    {
        errorBlob->Release();
        errorBlob = nullptr;
    }
    
    ID3D11Device* d3dDevice = (ID3D11Device*)graphics->D3DDevice();
    ID3DBlob* d3dBlob = (ID3DBlob*)blob;
    
    #ifdef SHOW_DISASSEMBLY
    ID3DBlob* asmBlob = nullptr;
    D3DDisassemble(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), 0, nullptr, &asmBlob);
    if (asmBlob)
    {
        String text((const char*)asmBlob->GetBufferPointer(), asmBlob->GetBufferSize());
        LOGINFOF("Shader %s disassembly: %s", FullName().CString(), text.CString());
        asmBlob->Release();
    }
    #endif

    if (stage == SHADER_VS)
    {
        elementHash = InspectInputSignature(d3dBlob);
        d3dDevice->CreateVertexShader(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), 0, (ID3D11VertexShader**)&shader);
    }
    else
        d3dDevice->CreatePixelShader(d3dBlob->GetBufferPointer(), d3dBlob->GetBufferSize(), 0, (ID3D11PixelShader**)&shader);

    if (!shader)
    {
        LOGERROR("Failed to create shader " + FullName());
        return false;
    }
    else
        LOGDEBUGF("Compiled shader %s bytecode size %u", FullName().CString(), (unsigned)d3dBlob->GetBufferSize());
    
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