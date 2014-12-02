// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Debug/Log.h"
#include "../../Debug/Profiler.h"
#include "GLGraphics.h"
#include "GLShaderProgram.h"
#include "GLShaderVariation.h"

#include <flextGL.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

ShaderProgram::ShaderProgram(ShaderVariation* vs_, ShaderVariation* ps_) :
    program(0),
    vs(vs_),
    ps(ps_),
    linked(false)
{
}

ShaderProgram::~ShaderProgram()
{
    Release();
}

void ShaderProgram::Release()
{
    if (program)
    {
        glDeleteProgram(program);
        program = 0;
    }

    linked = false;
}

bool ShaderProgram::Link()
{
    if (linked)
        return program != 0;

    PROFILE(LinkShaderProgram);

    // Do not retry without a Release() inbetween
    linked = true;

    if (!vs || !ps)
    {
        LOGERROR("Shader(s) are null, can not link shader program");
        return false;
    }

    if (!vs->ShaderObject() || !ps->ShaderObject())
    {
        LOGERROR("Shaders have not been compiled, can not link shader program");
        return false;
    }

    program = glCreateProgram();
    if (!program)
    {
        LOGERROR("Could not create shader program");
        return false;
    }

    glAttachShader(program, vs->ShaderObject());
    glAttachShader(program, ps->ShaderObject());
    glLinkProgram(program);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        int length, outLength;
        String errorString;

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        errorString.Resize(length);
        glGetProgramInfoLog(program, length, &outLength, &errorString[0]);
        glDeleteProgram(program);
        program = 0;

        LOGERRORF("Could not link shaders %s and %s: %s", vs->FullName().CString(), ps->FullName().CString(), errorString.CString());
        return false;
    }

    LOGDEBUGF("Linked shaders %s and %s", vs->FullName().CString(), ps->FullName().CString());
    return true;
}

ShaderVariation* ShaderProgram::VertexShader() const
{
    return vs;
}

ShaderVariation* ShaderProgram::PixelShader() const
{
    return ps;
}

}
