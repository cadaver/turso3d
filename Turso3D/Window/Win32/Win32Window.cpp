// For conditions of distribution and use, see copyright notice in License.txt

#include "../../Base/WString.h"
#include "../../Debug/Log.h"
#include "../../Math/Math.h"
#include "../Input.h"
#include "Win32Window.h"

#include <Windows.h>

#include "../../Debug/DebugNew.h"

namespace Turso3D
{

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);

String Window::className("Turso3DWindow");

Window::Window() :
    handle(nullptr),
    title("Turso3D Window"),
    windowStyle(0),
    size(IntVector2::ZERO),
    minimized(false),
    focus(false),
    resizable(false),
    fullscreen(false),
    inResize(false)
{
    RegisterSubsystem(this);

    // Cancel automatic DPI scaling
    BOOL(WINAPI* proc)() = (BOOL(WINAPI *)())(void*)GetProcAddress(GetModuleHandle("user32.dll"), "SetProcessDPIAware");
    if (proc)
        proc();
}

Window::~Window()
{
    Close();
    RemoveSubsystem(this);
}

void Window::SetTitle(const String& newTitle)
{
    title = newTitle;
    if (handle)
        SetWindowTextW((HWND)handle, WString(title).CString());
}

bool Window::SetSize(int width, int height, bool fullscreen_, bool resizable_)
{
    inResize = true;
    width = Max(width, 0);
    height = Max(height, 0);
    IntVector2 position(CW_USEDEFAULT, CW_USEDEFAULT);

    if (!fullscreen_)
    {
        windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
        if (resizable_)
            windowStyle |= WS_THICKFRAME | WS_MAXIMIZEBOX;

        // Return to desktop resolution if was fullscreen
        if (fullscreen)
            SetDisplayMode(0, 0);
    }
    else
    {
        windowStyle = WS_POPUP | WS_VISIBLE;
        position = IntVector2::ZERO;
        /// \todo Handle failure to set mode
        SetDisplayMode(width, height);
    }

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, windowStyle, false);

    if (!handle)
    {
        WNDCLASS wc;
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = GetModuleHandle(0);
        wc.hIcon = LoadIcon(0, IDI_APPLICATION);
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
        wc.lpszMenuName = nullptr;
        wc.lpszClassName = className.CString();

        RegisterClass(&wc);

        handle = CreateWindowW(WString(className).CString(), WString(title).CString(), windowStyle, position.x, position.y,
            rect.right - rect.left, rect.bottom - rect.top, 0, 0, GetModuleHandle(0), nullptr);
        if (!handle)
        {
            LOGERROR("Failed to create window");
            inResize = false;
            return false;
        }

        minimized = false;
        focus = false;

        SetWindowLongPtr((HWND)handle, GWLP_USERDATA, (LONG_PTR)this);
        ShowWindow((HWND)handle, SW_SHOW);
    }
    else
    {
        SetWindowLong((HWND)handle, GWL_STYLE, windowStyle);
        
        if (!fullscreen_)
        {
            WINDOWPLACEMENT placement;
            placement.length = sizeof(placement);
            GetWindowPlacement((HWND)handle, &placement);
            position = IntVector2(placement.rcNormalPosition.left, placement.rcNormalPosition.top);
        }

        SetWindowPos((HWND)handle, NULL, position.x, position.y, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
        ShowWindow((HWND)handle, SW_SHOW);
    }

    fullscreen = fullscreen_;
    resizable = resizable_;
    inResize = false;

    IntVector2 newSize = ClientRectSize();
    if (newSize != size)
    {
        size = newSize;
        resizeEvent.size = newSize;
        SendEvent(resizeEvent);
    }

    return true;
}

void Window::Close()
{
    if (handle)
    {
        // Return to desktop resolution if was fullscreen
        if (fullscreen)
            SetDisplayMode(0, 0);

        DestroyWindow((HWND)handle);
        handle = nullptr;
    }
}

void Window::Minimize()
{
    if (handle)
        ShowWindow((HWND)handle, SW_MINIMIZE);
}

void Window::Maximize()
{
    if (handle)
        ShowWindow((HWND)handle, SW_MAXIMIZE);
}

void Window::Restore()
{
    if (handle)
        ShowWindow((HWND)handle, SW_RESTORE);
}

void Window::PumpMessages()
{
    if (!handle)
        return;

    MSG msg;

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

bool Window::OnWindowMessage(unsigned msg, unsigned wParam, unsigned lParam)
{
    Input* input = Subsystem<Input>();
    bool handled = false;

    switch (msg)
    {
    case WM_DESTROY:
        handle = nullptr;
        break;

    case WM_CLOSE:
        SendEvent(closeRequestEvent);
        handled = true;
        break;

    case WM_ACTIVATE:
        {
            bool newFocus = LOWORD(wParam) != WA_INACTIVE;
            if (newFocus != focus)
            {
                focus = newFocus;
                if (focus)
                {
                    SendEvent(gainFocusEvent);
                    if (input)
                        input->OnGainFocus();
                }
                else
                {
                    SendEvent(loseFocusEvent);
                    if (input)
                        input->OnLoseFocus();

                    // If fullscreen, minimize on focus loss
                    if (fullscreen)
                        ShowWindow((HWND)handle, SW_MINIMIZE);
                }
            }
        }
        break;

    case WM_SIZE:
        {
            bool newMinimized = (wParam == SIZE_MINIMIZED);
            if (newMinimized != minimized)
            {
                minimized = newMinimized;
                if (minimized)
                {
                    // If is fullscreen, restore desktop resolution
                    if (fullscreen)
                        SetDisplayMode(0, 0);

                    SendEvent(minimizeEvent);
                }
                else
                {
                    // If should be fullscreen, restore mode now
                    if (fullscreen)
                        SetDisplayMode(size.x, size.y);

                    SendEvent(restoreEvent);
                }
            }

            if (!minimized && !inResize)
            {
                IntVector2 newSize = ClientRectSize();
                if (newSize != size)
                {
                    size = newSize;
                    resizeEvent.size = newSize;
                    SendEvent(resizeEvent);
                }
            }
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (input)
            input->OnKey(wParam, (lParam >> 16) & 0xff, true);
        handled = (msg == WM_KEYDOWN);
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (input)
            input->OnKey(wParam, (lParam >> 16) & 0xff, false);
        handled = (msg == WM_KEYUP);
        break;

    case WM_CHAR:
        if (input)
            input->OnChar(wParam);
        handled = true;
        break;

    case WM_MOUSEMOVE:
        if (input)
        {
            IntVector2 newPosition;
            newPosition.x = (int)(short)LOWORD(lParam);
            newPosition.y = (int)(short)HIWORD(lParam);
            input->OnMouseMove(newPosition);
        }
        handled = true;
        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (input)
        {
            unsigned button = (msg == WM_LBUTTONDOWN) ? MOUSEB_LEFT : (msg == WM_MBUTTONDOWN) ? MOUSEB_MIDDLE : MOUSEB_RIGHT;
            input->OnMouseButton(button, true);
            // Make sure we track the button release even if mouse moves outside the window
            SetCapture((HWND)handle);
        }
        handled = true;
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (input)
        {
            unsigned button = (msg == WM_LBUTTONUP) ? MOUSEB_LEFT : (msg == WM_MBUTTONUP) ? MOUSEB_MIDDLE : MOUSEB_RIGHT;
            input->OnMouseButton(button, false);
            // End capture when there are no more mouse buttons held down
            if (!input->MouseButtons())
                ReleaseCapture();
        }
        handled = true;
        break;
    }

    return handled;
}

IntVector2 Window::ClientRectSize() const
{
    if (handle)
    {
        RECT rect;
        GetClientRect((HWND)handle, &rect);
        return IntVector2(rect.right, rect.bottom);
    }
    else
        return IntVector2::ZERO;
}

void Window::SetDisplayMode(int width, int height)
{
    if (width && height)
    {
        DEVMODE screenMode;
        screenMode.dmSize = sizeof screenMode;
        screenMode.dmPelsWidth = width;
        screenMode.dmPelsHeight = height;
        screenMode.dmBitsPerPel = 32;
        screenMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettings(&screenMode, CDS_FULLSCREEN);
    }
    else
        ChangeDisplaySettings(nullptr, CDS_FULLSCREEN);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    bool handled = false;
    // When the window is just opening and has not assigned the userdata yet, let the default procedure handle the messages
    if (window)
        handled = window->OnWindowMessage(msg, (unsigned)wParam, (unsigned)lParam);
    return handled ? 0 : DefWindowProc(hwnd, msg, wParam, lParam);
}

}