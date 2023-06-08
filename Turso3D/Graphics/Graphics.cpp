// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "FrameBuffer.h"
#include "Graphics.h"
#include "IndexBuffer.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "UniformBuffer.h"
#include "VertexBuffer.h"

#include <SDL.h>
#include <glew.h>
#include <tracy/Tracy.hpp>

#ifdef WIN32
#include <Windows.h>
// Prefer the high-performance GPU on switchable GPU systems
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

static const unsigned glPrimitiveTypes[] =
{
    GL_LINES,
    GL_TRIANGLES
};

static const unsigned glCompareFuncs[] =
{
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const unsigned glSrcBlend[] =
{
    GL_ONE,
    GL_ONE,
    GL_DST_COLOR,
    GL_SRC_ALPHA,
    GL_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_DST_ALPHA,
    GL_ONE,
    GL_SRC_ALPHA
};

static const unsigned glDestBlend[] =
{
    GL_ZERO,
    GL_ONE,
    GL_ZERO,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ONE,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE,
    GL_ONE
};

static const unsigned glBlendOp[] =
{
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_ADD,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT
};

unsigned occlusionQueryType = GL_SAMPLES_PASSED;

Graphics::Graphics(const char* windowTitle, const IntVector2& windowSize) :
    window(nullptr),
    context(nullptr),
    lastBlendMode(MAX_BLEND_MODES),
    lastCullMode(MAX_CULL_MODES),
    lastDepthTest(MAX_COMPARE_MODES),
    lastColorWrite(true),
    lastDepthWrite(true),
    lastDepthBias(false),
    vsync(false),
    hasInstancing(false),
    instancingEnabled(false),
    lastFrameTime(0.0f)
{
    RegisterSubsystem(this);
    RegisterGraphicsLibrary();

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "system");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // On Windows and Intel, this will be converted (SDL2 hack) to SDL_GL_CONTEXT_PROFILE_COMPATIBILITY to avoid bugs like vsync failing to toggle multiple times
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_DisplayMode desktopMode;
    SDL_GetDesktopDisplayMode(0, &desktopMode);

    IntVector2 initialSize = windowSize;

    // If window size not specified or larger than desktop, use desktop size instead
    if (!initialSize.x || !initialSize.y || initialSize.x > desktopMode.w || initialSize.y > desktopMode.h)
        initialSize = IntVector2(desktopMode.w, desktopMode.h);

    window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, initialSize.x, initialSize.y, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
}

Graphics::~Graphics()
{
    if (context)
    {
        SDL_GL_DeleteContext(context);
        context = nullptr;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
    RemoveSubsystem(this);
}

bool Graphics::Initialize()
{
    ZoneScoped;

    if (context)
        return true;

    if (!window)
    {
        LOGERROR("Window not opened");
        return false;
    }

    context = SDL_GL_CreateContext(window);
    if (!context)
    {
        LOGERROR("Could not create OpenGL 3.2 context");
        return false;
    }

    GLenum err = glewInit();
    if (err != GLEW_OK || !GLEW_VERSION_3_2)
    {
        LOGERROR("Could not initialize OpenGL 3.2");
        return false;
    }

    // "Any samples passed" is potentially faster if supported
    if (GLEW_VERSION_3_3)
        occlusionQueryType = GL_ANY_SAMPLES_PASSED;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearDepth(1.0f);
    glDepthRange(0.0f, 1.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    GLuint defaultVao;
    glGenVertexArrays(1, &defaultVao);
    glBindVertexArray(defaultVao);

    // Use texcoords 3-5 for instancing if supported
    if (glVertexAttribDivisorARB)
    {
        hasInstancing = true;

        glVertexAttribDivisorARB(ATTR_TEXCOORD3, 1);
        glVertexAttribDivisorARB(ATTR_TEXCOORD4, 1);
        glVertexAttribDivisorARB(ATTR_TEXCOORD5, 1);
    }

    DefineQuadVertexBuffer();

    SetVSync(vsync);
    frameTimer.Reset();

    return true;
}

void Graphics::Resize(const IntVector2& size)
{
    SDL_SetWindowSize(window, size.x, size.y);
}

void Graphics::SetFullscreen(bool enable)
{
    SDL_SetWindowFullscreen(window, enable ? SDL_WINDOW_FULLSCREEN : 0);
}

void Graphics::SetVSync(bool enable)
{
    if (IsInitialized())
    {
        SDL_GL_SetSwapInterval(enable ? 1 : 0);
        vsync = enable;
    }
}

void Graphics::Present()
{
    ZoneScoped;

    SDL_GL_SwapWindow(window);

    lastFrameTime = 0.000001f * frameTimer.ElapsedUSec();
    frameTimer.Reset();
}

void Graphics::SetFrameBuffer(FrameBuffer* buffer)
{
    if (buffer)
        buffer->Bind();
    else
        FrameBuffer::Unbind();
}

void Graphics::SetViewport(const IntRect& viewRect)
{
    glViewport(viewRect.left, viewRect.top, viewRect.right - viewRect.left, viewRect.bottom - viewRect.top);
}

ShaderProgram* Graphics::SetProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
{
    ShaderProgram* program = CreateProgram(shaderName, vsDefines, fsDefines);
    if (program && program->Bind())
        return program;
    else
        return nullptr;
}

ShaderProgram* Graphics::CreateProgram(const std::string& shaderName, const std::string& vsDefines, const std::string& fsDefines)
{
    ResourceCache* cache = Subsystem<ResourceCache>();
    Shader* shader = cache->LoadResource<Shader>(shaderName);
    if (shader)
        return shader->CreateProgram(vsDefines, fsDefines);
    else
        return nullptr;
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, float value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniform1f(location, value);
    }
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector2& value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniform2fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector3& value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniform3fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Vector4& value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniform4fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix3x4& value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, PresetUniform uniform, const Matrix4& value)
{
    if (program)
    {
        int location = program->Uniform(uniform);
        if (location >= 0)
            glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, float value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform1f(location, value);
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector2& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform2fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector3& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform3fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, const Vector4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniform4fv(location, 1, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, const Matrix3x4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniformMatrix3x4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Graphics::SetUniform(ShaderProgram* program, const char* name, const Matrix4& value)
{
    if (program)
    {
        int location = program->Uniform(name);
        if (location >= 0)
            glUniformMatrix4fv(location, 1, GL_FALSE, value.Data());
    }
}

void Graphics::SetUniformBuffer(size_t index, UniformBuffer* buffer)
{
    if (buffer)
        buffer->Bind(index);
    else
        UniformBuffer::Unbind(index);
}

void Graphics::SetTexture(size_t index, Texture* texture)
{
    if (texture)
        texture->Bind(index);
    else
        Texture::Unbind(index);
}

void Graphics::SetVertexBuffer(VertexBuffer* buffer, ShaderProgram* program)
{
    if (buffer && program)
        buffer->Bind(program->Attributes());
}

void Graphics::SetIndexBuffer(IndexBuffer* buffer)
{
    if (buffer)
        buffer->Bind();
}

void Graphics::SetRenderState(BlendMode blendMode, CullMode cullMode, CompareMode depthTest, bool colorWrite, bool depthWrite)
{
    if (blendMode != lastBlendMode)
    {
        if (blendMode == BLEND_REPLACE)
            glDisable(GL_BLEND);
        else
        {
            glEnable(GL_BLEND);
            glBlendFunc(glSrcBlend[blendMode], glDestBlend[blendMode]);
            glBlendEquation(glBlendOp[blendMode]);
        }

        lastBlendMode = blendMode;
    }

    if (cullMode != lastCullMode)
    {
        if (cullMode == CULL_NONE)
            glDisable(GL_CULL_FACE);
        else
        {
            // Use Direct3D convention, ie. clockwise vertices define a front face
            glEnable(GL_CULL_FACE);
            glCullFace(cullMode == CULL_BACK ? GL_FRONT : GL_BACK);
        }

        lastCullMode = cullMode;
    }

    if (depthTest != lastDepthTest)
    {
        glDepthFunc(glCompareFuncs[depthTest]);
        lastDepthTest = depthTest;
    }

    if (colorWrite != lastColorWrite)
    {
        GLboolean newColorWrite = colorWrite ? GL_TRUE : GL_FALSE;
        glColorMask(newColorWrite, newColorWrite, newColorWrite, newColorWrite);
        lastColorWrite = colorWrite;
    }

    if (depthWrite != lastDepthWrite)
    {
        GLboolean newDepthWrite = depthWrite ? GL_TRUE : GL_FALSE;
        glDepthMask(newDepthWrite);
        lastDepthWrite = depthWrite;
    }
}

void Graphics::SetDepthBias(float constantBias, float slopeScaleBias)
{
    if (constantBias <= 0.0f && slopeScaleBias <= 0.0f)
    {
        if (lastDepthBias)
        {
            glDisable(GL_POLYGON_OFFSET_FILL);
            lastDepthBias = false;
        }
    }
    else
    {
        if (!lastDepthBias)
        {
            glEnable(GL_POLYGON_OFFSET_FILL);
            lastDepthBias = true;
        }

        glPolygonOffset(slopeScaleBias, constantBias);
    }
}

void Graphics::Clear(bool clearColor, bool clearDepth, const IntRect& clearRect, const Color& backgroundColor)
{
    if (clearColor)
    {
        glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        lastColorWrite = true;
    }
    if (clearDepth)
    {
        glDepthMask(GL_TRUE);
        lastDepthWrite = true;
    }

    GLenum glClearBits = 0;
    if (clearColor)
        glClearBits |= GL_COLOR_BUFFER_BIT;
    if (clearDepth)
        glClearBits |= GL_DEPTH_BUFFER_BIT;

    if (clearRect == IntRect::ZERO)
        glClear(glClearBits);
    else
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(clearRect.left, clearRect.top, clearRect.right - clearRect.left, clearRect.bottom - clearRect.top);
        glClear(glClearBits);
        glDisable(GL_SCISSOR_TEST);
    }
}

void Graphics::Blit(FrameBuffer* dest, const IntRect& destRect, FrameBuffer* src, const IntRect& srcRect, bool blitColor, bool blitDepth, TextureFilterMode filter)
{
    FrameBuffer::Bind(dest, src);

    GLenum glBlitBits = 0;
    if (blitColor)
        glBlitBits |= GL_COLOR_BUFFER_BIT;
    if (blitDepth)
        glBlitBits |= GL_DEPTH_BUFFER_BIT;

    glBlitFramebuffer(srcRect.left, srcRect.top, srcRect.right, srcRect.bottom, destRect.left, destRect.top, destRect.right, destRect.bottom, glBlitBits, filter == FILTER_POINT ? GL_NEAREST : GL_LINEAR);
}

void Graphics::Draw(PrimitiveType type, size_t drawStart, size_t drawCount)
{
    if (instancingEnabled)
    {
        glDisableVertexAttribArray(ATTR_TEXCOORD3);
        glDisableVertexAttribArray(ATTR_TEXCOORD4);
        glDisableVertexAttribArray(ATTR_TEXCOORD5);
        instancingEnabled = false;
    }

    glDrawArrays(glPrimitiveTypes[type], (GLsizei)drawStart, (GLsizei)drawCount);
}

void Graphics::DrawIndexed(PrimitiveType type, size_t drawStart, size_t drawCount)
{
    if (instancingEnabled)
    {
        glDisableVertexAttribArray(ATTR_TEXCOORD3);
        glDisableVertexAttribArray(ATTR_TEXCOORD4);
        glDisableVertexAttribArray(ATTR_TEXCOORD5);
        instancingEnabled = false;
    }

    unsigned indexSize = (unsigned)IndexBuffer::BoundIndexSize();
    if (indexSize)
        glDrawElements(glPrimitiveTypes[type], (GLsizei)drawCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(drawStart * indexSize));
}

void Graphics::DrawInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount)
{
    if (!hasInstancing)
        return;

    if (instanceVertexBuffer)
    {
        if (!instancingEnabled)
        {
            glEnableVertexAttribArray(ATTR_TEXCOORD3);
            glEnableVertexAttribArray(ATTR_TEXCOORD4);
            glEnableVertexAttribArray(ATTR_TEXCOORD5);
            instancingEnabled = true;
        }

        unsigned instanceVertexSize = (unsigned)instanceVertexBuffer->VertexSize();

        instanceVertexBuffer->Bind(0);
        glVertexAttribPointer(ATTR_TEXCOORD3, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize));
        glVertexAttribPointer(ATTR_TEXCOORD4, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + sizeof(Vector4)));
        glVertexAttribPointer(ATTR_TEXCOORD5, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + 2 * sizeof(Vector4)));
        glDrawArraysInstanced(glPrimitiveTypes[type], (GLint)drawStart, (GLsizei)drawCount, (GLsizei)instanceCount);
    }
}

void Graphics::DrawIndexedInstanced(PrimitiveType type, size_t drawStart, size_t drawCount, VertexBuffer* instanceVertexBuffer, size_t instanceStart, size_t instanceCount)
{
    if (!hasInstancing)
        return;

    unsigned indexSize = (unsigned)IndexBuffer::BoundIndexSize();

    if (indexSize && instanceVertexBuffer)
    {
        if (!instancingEnabled)
        {
            glEnableVertexAttribArray(ATTR_TEXCOORD3);
            glEnableVertexAttribArray(ATTR_TEXCOORD4);
            glEnableVertexAttribArray(ATTR_TEXCOORD5);
            instancingEnabled = true;
        }

        unsigned instanceVertexSize = (unsigned)instanceVertexBuffer->VertexSize();

        instanceVertexBuffer->Bind(0);
        glVertexAttribPointer(ATTR_TEXCOORD3, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize));
        glVertexAttribPointer(ATTR_TEXCOORD4, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + sizeof(Vector4)));
        glVertexAttribPointer(ATTR_TEXCOORD5, 4, GL_FLOAT, GL_FALSE, instanceVertexSize, (const void*)(instanceStart * instanceVertexSize + 2 * sizeof(Vector4)));
        glDrawElementsInstanced(glPrimitiveTypes[type], (GLsizei)drawCount, indexSize == sizeof(unsigned short) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (const void*)(drawStart * indexSize), (GLsizei)instanceCount);
    }
}

void Graphics::DrawQuad()
{
    quadVertexBuffer->Bind(MASK_POSITION | MASK_TEXCOORD);
    Draw(PT_TRIANGLE_LIST, 0, 6);
}

unsigned Graphics::BeginOcclusionQuery(void* object)
{
    GLuint queryId;

    if (freeQueries.size())
    {
        queryId = freeQueries.back();
        freeQueries.pop_back();
    }
    else
        glGenQueries(1, &queryId);

    glBeginQuery(occlusionQueryType, queryId);
    pendingQueries.push_back(std::make_pair(queryId, object));

    return queryId;
}

void Graphics::EndOcclusionQuery()
{
    glEndQuery(occlusionQueryType);
}


void Graphics::FreeOcclusionQuery(unsigned queryId)
{
    if (!queryId)
        return;

    for (auto it = pendingQueries.begin(); it != pendingQueries.end(); ++it)
    {
        if (it->first == queryId)
        {
            pendingQueries.erase(it);
            break;
        }
    }

    glDeleteQueries(1, &queryId);
}

void Graphics::CheckOcclusionQueryResults(std::vector<OcclusionQueryResult>& result)
{
    ZoneScoped;

    GLuint available = 0;

    // To save API calls, go through queries in reverse order and assume that if a later query has its result available, then all earlier queries will have too
    for (size_t i = pendingQueries.size() - 1; i < pendingQueries.size(); --i)
    {
        GLuint queryId = pendingQueries[i].first;

        if (!available)
            glGetQueryObjectuiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);

        if (available)
        {
            GLuint passed = 0;
            glGetQueryObjectuiv(queryId, GL_QUERY_RESULT, &passed);

            OcclusionQueryResult newResult;
            newResult.id = queryId;
            newResult.object = pendingQueries[i].second;
            newResult.visible = passed > 0;
            result.push_back(newResult);

            freeQueries.push_back(queryId);
            pendingQueries.erase(pendingQueries.begin() + i);
        }
    }
}

IntVector2 Graphics::Size() const
{
    IntVector2 size;
    SDL_GetWindowSize(window, &size.x, &size.y);
    return size;
}

IntVector2 Graphics::RenderSize() const
{
    IntVector2 size;
    SDL_GL_GetDrawableSize(window, &size.x, &size.y);
    return size;
}

bool Graphics::IsFullscreen() const
{
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Graphics::DefineQuadVertexBuffer()
{
    float quadVertexData[] = {
        // Position         // UV
        -1.0f, 1.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f
    };

    std::vector<VertexElement> vertexDeclaration;
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR3, SEM_POSITION));
    vertexDeclaration.push_back(VertexElement(ELEM_VECTOR2, SEM_TEXCOORD));
    quadVertexBuffer = new VertexBuffer();
    quadVertexBuffer->Define(USAGE_DEFAULT, 6, vertexDeclaration, quadVertexData);
}

void RegisterGraphicsLibrary()
{
    static bool registered = false;
    if (registered)
        return;

    Texture::RegisterObject();
    Shader::RegisterObject();

    registered = true;
}
