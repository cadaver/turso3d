// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "Graphics.h"
#include "ShaderProgram.h"

#include <glew.h>
#include <tracy/Tracy.hpp>

#include <cctype>

static ShaderProgram* boundProgram = nullptr;

const size_t MAX_NAME_LENGTH = 256;

const char* attribNames[] =
{
    "position",
    "normal",
    "tangent",
    "color",
    "texCoord",
    "texCoord1",
    "texCoord2",
    "texCoord3",
    "texCoord4",
    "texCoord5",
    "blendWeights",
    "blendIndices"
};

void CommentOutFunction(std::string& code, const std::string& signature)
{
    size_t startPos = code.find(signature);
    size_t braceLevel = 0;
    if (startPos == std::string::npos)
        return;

    code.insert(startPos, "/*");

    for (size_t i = startPos + 2 + signature.length(); i < code.length(); ++i)
    {
        if (code[i] == '{')
            ++braceLevel;
        else if (code[i] == '}')
        {
            --braceLevel;
            if (!braceLevel)
            {
                code.insert(i + 1, "*/");
                return;
            }
        }
    }
}

int NumberPostfix(const std::string& string)
{
    for (size_t i = 0; i < string.length(); ++i)
    {
        if (isdigit(string[i]))
            return ParseInt(string.c_str() + i);
    }

    return -1;
}

ShaderProgram::ShaderProgram(const std::string& sourceCode, const std::string& shaderName_, const std::string& vsDefines, const std::string& fsDefines) :
    program(0)
{
    assert(Object::Subsystem<Graphics>()->IsInitialized());

    shaderName = vsDefines.length() ? (shaderName_ + " " + vsDefines + " " + fsDefines) : (shaderName_ + " " + fsDefines);

    Create(sourceCode, Split(vsDefines), Split(fsDefines));
}

ShaderProgram::~ShaderProgram()
{
    // Context may be gone at destruction time. In this case just no-op the cleanup
    if (Object::Subsystem<Graphics>())
        Release();
}

int ShaderProgram::Uniform(const std::string& name) const
{
    return Uniform(StringHash(name));
}

int ShaderProgram::Uniform(const char* name) const
{
    return Uniform(StringHash(name));
}

int ShaderProgram::Uniform(StringHash name) const
{
    auto it = uniforms.find(name);
    return it != uniforms.end() ? it->second : -1;
}

bool ShaderProgram::Bind()
{
    if (!program)
        return false;

    if (boundProgram == this)
        return true;

    glUseProgram(program);
    boundProgram = this;
    return true;
}

void ShaderProgram::Create(const std::string& sourceCode, const std::vector<std::string>& vsDefines, const std::vector<std::string>& fsDefines)
{
    ZoneScoped;

    std::string vsSourceCode;
    vsSourceCode += "#version 150\n";
    vsSourceCode += "#define COMPILEVS\n";
    for (size_t i = 0; i < vsDefines.size(); ++i)
    {
        vsSourceCode += "#define ";
        vsSourceCode += Replace(vsDefines[i], '=', ' ');
        vsSourceCode += "\n";
    }
    vsSourceCode += sourceCode;
    CommentOutFunction(vsSourceCode, "void frag(");
    ReplaceInPlace(vsSourceCode, "void vert(", "void main(");
    const char* vsShaderStr = vsSourceCode.c_str();

    int vsCompiled;
    unsigned vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsShaderStr, nullptr);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &vsCompiled);

    {
        int length, outLength;
        std::string errorString;

        glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &length);
        errorString.resize(length);
        glGetShaderInfoLog(vs, 1024, &outLength, &errorString[0]);

        if (!vsCompiled)
            LOGERRORF("VS %s compile error: %s", shaderName.c_str(), errorString.c_str());
#ifdef _DEBUG
        else if (length > 1)
            LOGDEBUGF("VS %s compile output: %s", shaderName.c_str(), errorString.c_str());
#endif
    }

    std::string fsSourceCode;
    fsSourceCode += "#version 150\n";
    fsSourceCode += "#define COMPILEFS\n";
    for (size_t i = 0; i < fsDefines.size(); ++i)
    {
        fsSourceCode += "#define ";
        fsSourceCode += Replace(fsDefines[i], '=', ' ');
        fsSourceCode += "\n";
    }
    fsSourceCode += sourceCode;
    CommentOutFunction(fsSourceCode, "void vert(");
    ReplaceInPlace(fsSourceCode, "void frag(", "void main(");
    const char* fsShaderStr = fsSourceCode.c_str();

    int fsCompiled;
    unsigned fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsShaderStr, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &fsCompiled);

    {
        int length, outLength;
        std::string errorString;

        glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &length);
        errorString.resize(length);
        glGetShaderInfoLog(fs, 1024, &outLength, &errorString[0]);

        if (!fsCompiled)
            LOGERRORF("FS %s compile error: %s", shaderName.c_str(), errorString.c_str());
#ifdef _DEBUG
        else if (length > 1)
            LOGDEBUGF("FS %s compile output: %s", shaderName.c_str(), errorString.c_str());
#endif
    }

    if (!vsCompiled || !fsCompiled)
    {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    for (unsigned i = 0; i < MAX_VERTEX_ATTRIBUTES; ++i)
        glBindAttribLocation(program, i, attribNames[i]);

    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    int linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    {
        int length, outLength;
        std::string errorString;

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        errorString.resize(length);
        glGetProgramInfoLog(program, length, &outLength, &errorString[0]);

        if (!linked)
        {
            LOGERRORF("Could not link shader %s: %s", shaderName.c_str(), errorString.c_str());
            glDeleteProgram(program);
            program = 0;
            return;
        }
#ifdef _DEBUG
        else if (length > 1)
            LOGDEBUGF("Shader %s link messages: %s", shaderName.c_str(), errorString.c_str());
#endif
    }

    char nameBuffer[MAX_NAME_LENGTH];
    int numAttributes, numUniforms, nameLength, numElements, numUniformBlocks;
    GLenum type;

    attributes = 0;

    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttributes);
    for (int i = 0; i < numAttributes; ++i)
    {
        glGetActiveAttrib(program, i, (GLsizei)MAX_NAME_LENGTH, &nameLength, &numElements, &type, nameBuffer);

        std::string name(nameBuffer, nameLength);
        size_t attribIndex = ListIndex(name.c_str(), attribNames, 0xfffffff);
        if (attribIndex < 32)
            attributes |= (1 << attribIndex);
    }

    uniforms.clear();

    Bind();
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);

    for (size_t i = 0; i < MAX_PRESET_UNIFORMS; ++i)
        presetUniforms[i] = -1;

    for (int i = 0; i < numUniforms; ++i)
    {
        glGetActiveUniform(program, i, MAX_NAME_LENGTH, &nameLength, &numElements, &type, nameBuffer);
        std::string name(nameBuffer, nameLength);

        int location = glGetUniformLocation(program, name.c_str());
        ReplaceInPlace(name, "[0]", "");
        uniforms[StringHash(name)] = location;

        // Check if uniform is a preset one for quick access
        PresetUniform preset = (PresetUniform)ListIndex(name.c_str(), presetUniformNames, MAX_PRESET_UNIFORMS);
        if (preset < MAX_PRESET_UNIFORMS)
            presetUniforms[preset] = location;

        if ((type >= GL_SAMPLER_1D && type <= GL_SAMPLER_2D_SHADOW) || (type >= GL_SAMPLER_1D_ARRAY && type <= GL_SAMPLER_CUBE_SHADOW) || (type >= GL_INT_SAMPLER_1D && type <= GL_UNSIGNED_INT_SAMPLER_2D_ARRAY))
        {
            // Assign sampler uniforms to a texture unit according to the number appended to the sampler name
            int unit = NumberPostfix(name);

            if (unit < 0)
                continue;

            // Array samplers may have multiple elements, assign each sequentially
            if (numElements > 1)
            {
                std::vector<int> units;
                for (int j = 0; j < numElements; ++j)
                    units.push_back(unit++);
                glUniform1iv(location, numElements, &units[0]);
            }
            else
                glUniform1iv(location, 1, &unit);
        }
    }

    glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numUniformBlocks);
    for (int i = 0; i < numUniformBlocks; ++i)
    {
        glGetActiveUniformBlockName(program, i, MAX_NAME_LENGTH, &nameLength, nameBuffer);
        std::string name(nameBuffer, nameLength);

        int blockIndex = glGetUniformBlockIndex(program, name.c_str());
        int bindingIndex = NumberPostfix(name);
        // If no number postfix in the name, use the block index
        if (bindingIndex < 0)
            bindingIndex = blockIndex;

        glUniformBlockBinding(program, blockIndex, bindingIndex);
    }

    LOGDEBUGF("Linked shader program %s", shaderName.c_str());
}

void ShaderProgram::Release()
{
    if (program)
    {
        glDeleteProgram(program);
        program = 0;

        if (boundProgram == this)
            boundProgram = nullptr;
    }
}
