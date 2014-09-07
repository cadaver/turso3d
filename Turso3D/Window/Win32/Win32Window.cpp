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

Window::Window() :
    handle(0),
    title("Turso3D Window"),
    wasMinimized(false)
{
    RegisterSubsystem(this);
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

bool Window::SetSize(int width, int height, bool resizable)
{
    width = Max(width, 0);
    height = Max(height, 0);
    DWORD windowStyle = WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX;
    if (resizable)
        windowStyle |= WS_THICKFRAME | WS_MAXIMIZEBOX;

    if (!handle)
    {
        // Cancel automatic DPI scaling
        BOOL (WINAPI* proc)() = (BOOL (WINAPI *)())(void*)GetProcAddress(GetModuleHandle("user32.dll"), "SetProcessDPIAware");
        if (proc)
            proc();
        
        String className("Turso3DWindow");

        WNDCLASS wc;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = GetModuleHandle(0);
        wc.hIcon = LoadIcon(0, IDI_APPLICATION);
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
        wc.lpszMenuName = 0;
        wc.lpszClassName = className.CString();

        RegisterClass(&wc);

        RECT rect = { 0, 0, width, height };
        AdjustWindowRect(&rect, windowStyle, false);
        handle = CreateWindowW(WString(className).CString(), WString(title).CString(), windowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            rect.right, rect.bottom, 0, 0, GetModuleHandle(0), 0);
        if (!handle)
        {
            LOGERROR("Failed to create window");
            return false;
        }
        wasMinimized = false;

        SetWindowLongPtr((HWND)handle, GWLP_USERDATA, (LONG)this);
        ShowWindow((HWND)handle, SW_SHOW);
        return true;
    }
    else
    {
        SetWindowLong((HWND)handle, GWL_STYLE, windowStyle);
        WINDOWPLACEMENT placement;
        placement.length = sizeof(placement);
        GetWindowPlacement((HWND)handle, &placement);
        SetWindowPos((HWND)handle, HWND_TOP, placement.rcNormalPosition.left, placement.rcNormalPosition.top, width, height, SWP_NOZORDER);
        return true;
    }
}

void Window::Close()
{
    if (handle)
    {
        DestroyWindow((HWND)handle);
        handle = 0;
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
    MSG msg;

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int Window::Width() const
{
    if (handle)
    {
        RECT rect;
        GetClientRect((HWND)handle, &rect);
        return rect.right;
    }
    else
        return 0;
}

int Window::Height() const
{
    if (handle)
    {
        RECT rect;
        GetClientRect((HWND)handle, &rect);
        return rect.bottom;
    }
    else
        return 0;
}

bool Window::IsResizable() const
{
    if (handle)
    {
        DWORD windowStyle = GetWindowLong((HWND)handle, GWL_STYLE);
        return (windowStyle & WS_THICKFRAME) != 0;
    }
    else
        return false;
}

bool Window::IsMinimized() const
{
    return handle ? IsIconic((HWND)handle) == TRUE : false;
}

bool Window::HasFocus() const
{
    return GetActiveWindow() == (HWND)handle;
}

void Window::OnClose()
{
    handle = 0;
}

void Window::OnSizeChange(unsigned mode)
{
    bool minimized = mode == SIZE_MINIMIZED;
    if (minimized)
        SendEvent(minimizeEvent);
    else if (wasMinimized)
        SendEvent(restoreEvent);
    wasMinimized = minimized;

    if (!minimized)
    {
        RECT rect;
        GetClientRect((HWND)handle, &rect);
        resizeEvent.width = rect.right;
        resizeEvent.height = rect.bottom;
        SendEvent(resizeEvent);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    // When the window is just opening and has not assigned the userdata yet, let the default procedure handle the messages
    if (!window)
        return DefWindowProc(hwnd, msg, wParam, lParam);

    Input* input = Object::Subsystem<Input>();
    bool handled = false;

    switch (msg)
    {
    case WM_DESTROY:
        window->OnClose();
        break;

    case WM_CLOSE:
        window->SendEvent(window->closeRequestEvent);
        handled = true;
        break;

    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            if (input)
                input->OnGainFocus();
            window->SendEvent(window->gainFocusEvent);
        }
        else
        {
            if (input)
                input->OnLoseFocus();
            window->SendEvent(window->loseFocusEvent);
        }
        break;

    case WM_SIZE:
        window->OnSizeChange((unsigned)wParam);
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (input)
            input->OnKey((unsigned)wParam, (lParam >> 16) & 0xff, true);
        handled = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (input)
            input->OnKey((unsigned)wParam, (lParam >> 16) & 0xff, false);
        handled = false;
        break;

    case WM_CHAR:
        if (input)
            input->OnChar((unsigned)wParam);
        handled = false;
        break;

    case WM_MOUSEMOVE:
        {
            IntVector2 newPosition;
            newPosition.x = (int)(short)LOWORD(lParam);
            newPosition.y = (int)(short)HIWORD(lParam);
            if (input)
                input->OnMouseMove(newPosition);
        }
        break;

    case WM_LBUTTONDOWN:
        if (input)
            input->OnMouseButton(MOUSEB_LEFT, true);
        handled = true;
        break;

    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP:
        if (input)
            input->OnMouseButton(MOUSEB_LEFT, false);
        handled = true;
        break;

    case WM_RBUTTONDOWN:
        if (input)
            input->OnMouseButton(MOUSEB_RIGHT, true);
        handled = true;
        break;

    case WM_NCRBUTTONUP:
    case WM_RBUTTONUP:
        if (input)
            input->OnMouseButton(MOUSEB_RIGHT, false);
        handled = true;
        break;

    case WM_MBUTTONDOWN:
        if (input)
            input->OnMouseButton(MOUSEB_MIDDLE, true);
        handled = true;
        break;

    case WM_NCMBUTTONUP:
    case WM_MBUTTONUP:
        if (input)
            input->OnMouseButton(MOUSEB_MIDDLE, false);
        handled = true;
        break;
    }

    return handled ? 0 : DefWindowProc(hwnd, msg, wParam, lParam);
}

}