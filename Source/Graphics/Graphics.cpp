// For conditions of distribution and use, see copyright notice in License.txt

#include "../IO/Log.h"
#include "../Thread/Profiler.h"
#include "Graphics.h"
#include "Shader.h"
#include "Texture.h"

#include <SDL.h>
#include <glew.h>

#ifdef WIN32
#include <Windows.h>
// Prefer the high-performance GPU on switchable GPU systems
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

Graphics::Graphics(const char* windowTitle, const IntVector2& windowSize) :
    window(nullptr),
    context(nullptr),
    vsync(false)
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowSize.x, windowSize.y, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
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
    PROFILE(InitializeGraphics);

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

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glClearDepth(1.0f);
    glDepthRange(0.0f, 1.0f);

    GLuint defaultVao;
    glGenVertexArrays(1, &defaultVao);
    glBindVertexArray(defaultVao);

    SetVSync(vsync);

    return true;
}

void Graphics::SetWindowSize(const IntVector2& size)
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
    PROFILE(Present);

    SDL_GL_SwapWindow(window);
}

IntVector2 Graphics::WindowSize() const
{
    IntVector2 size;
    SDL_GetWindowSize(window, &size.x, &size.y);
    return size;
}

int Graphics::Width() const
{
    return WindowSize().x;
}

int Graphics::Height() const
{
    return WindowSize().y;
}

bool Graphics::IsFullscreen() const
{
    unsigned flags = SDL_GetWindowFlags(window);
    return (flags & SDL_WINDOW_FULLSCREEN) != 0;
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
