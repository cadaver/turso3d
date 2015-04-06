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

// Handle missing touch input definitions (MinGW)
#if WINVER < 0x0601
#define TWF_FINETOUCH 1
#define TWF_WANTPALM 2
#define TOUCHEVENTF_MOVE 0x1
#define TOUCHEVENTF_DOWN 0x2
#define TOUCHEVENTF_UP 0x4
#define WM_TOUCH 0x240

DECLARE_HANDLE(HTOUCHINPUT);

/// \cond PRIVATE
typedef struct {
    LONG x;
    LONG y;
    HANDLE hSource;
    DWORD dwID;
    DWORD dwFlags;
    DWORD dwMask;
    DWORD dwTime;
    ULONG_PTR dwExtraInfo;
    DWORD cxContact;
    DWORD cyContact;
} TOUCHINPUT, *PTOUCHINPUT;
/// \endcond
#endif

static BOOL(WINAPI* setProcessDpiAware)() = nullptr;
static BOOL(WINAPI* registerTouchWindow)(HWND, ULONG) = nullptr;
static BOOL(WINAPI* getTouchInputInfo)(HTOUCHINPUT, UINT, PTOUCHINPUT, int) = nullptr;
static BOOL(WINAPI* closeTouchInputHandle)(HTOUCHINPUT) = nullptr;
static bool functionsInitialized = false;

static LRESULT CALLBACK WndProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);

String Window::className("Turso3DWindow");

Window::Window() :
    handle(nullptr),
    title("Turso3D Window"),
    size(IntVector2::ZERO),
    savedPosition(IntVector2(M_MIN_INT, M_MIN_INT)),
    mousePosition(IntVector2::ZERO),
    windowStyle(0),
    minimized(false),
    focus(false),
    resizable(false),
    fullscreen(false),
    inResize(false),
    mouseVisible(true),
    mouseVisibleInternal(true)
{
    RegisterSubsystem(this);

    if (!functionsInitialized)
    {
        HMODULE userDll = GetModuleHandle("user32.dll");
        setProcessDpiAware = (BOOL(WINAPI*)())(void*)GetProcAddress(userDll, "SetProcessDPIAware");
        registerTouchWindow = (BOOL(WINAPI*)(HWND, ULONG))(void*)GetProcAddress(userDll, "RegisterTouchWindow");
        getTouchInputInfo = (BOOL(WINAPI*)(HTOUCHINPUT, UINT, PTOUCHINPUT, int))(void*)GetProcAddress(userDll, "GetTouchInputInfo");
        closeTouchInputHandle = (BOOL(WINAPI*)(HTOUCHINPUT))(void*)GetProcAddress(userDll, "CloseTouchInputHandle");

        // Cancel automatic DPI scaling
        if (setProcessDpiAware)
            setProcessDpiAware();
        
        functionsInitialized = true;
    }
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

bool Window::SetSize(const IntVector2& size_, bool fullscreen_, bool resizable_)
{
    inResize = true;
    IntVector2 position = savedPosition;

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
        // When switching to fullscreen, save last windowed mode position
        if (!fullscreen)
            savedPosition = Position();

        windowStyle = WS_POPUP | WS_VISIBLE;
        position = IntVector2::ZERO;
        /// \todo Handle failure to set mode
        SetDisplayMode(size_.x, size_.y);
    }

    RECT rect = {0, 0, size_.x, size_.y};
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

        // Enable touch input if available
        if (registerTouchWindow)
            registerTouchWindow((HWND)handle, TWF_FINETOUCH | TWF_WANTPALM);

        minimized = false;
        focus = false;

        SetWindowLongPtr((HWND)handle, GWLP_USERDATA, (LONG_PTR)this);
        ShowWindow((HWND)handle, SW_SHOW);
    }
    else
    {
        SetWindowLong((HWND)handle, GWL_STYLE, windowStyle);
        
        if (!fullscreen_ && (savedPosition.x == M_MIN_INT || savedPosition.y == M_MIN_INT))
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

    UpdateMouseVisible();
    UpdateMousePosition();

    return true;
}

void Window::SetPosition(const IntVector2& position)
{
    if (handle)
        SetWindowPos((HWND)handle, NULL, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void Window::SetMouseVisible(bool enable)
{
    if (enable != mouseVisible)
    {
        mouseVisible = enable;
        UpdateMouseVisible();
    }
}

void Window::SetMousePosition(const IntVector2& position)
{
    if (handle)
    {
        mousePosition = position;
        POINT screenPosition;
        screenPosition.x = position.x;
        screenPosition.y = position.y;
        ClientToScreen((HWND)handle, &screenPosition);
        SetCursorPos(screenPosition.x, screenPosition.y);
    }
}

void Window::Close()
{
    if (handle)
    {
        // Return to desktop resolution if was fullscreen, else save last windowed mode position
        if (fullscreen)
            SetDisplayMode(0, 0);
        else
            savedPosition = Position();

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

IntVector2 Window::Position() const
{
    if (handle)
    {
        RECT rect;
        GetWindowRect((HWND)handle, &rect);
        return IntVector2(rect.left, rect.top);
    }
    else
        return IntVector2::ZERO;
}

bool Window::OnWindowMessage(unsigned msg, unsigned wParam, unsigned lParam)
{
    Input* input = Subsystem<Input>();
    bool handled = false;

    // Skip emulated mouse events that are caused by touch
    bool emulatedMouse = (GetMessageExtraInfo() & 0xffffff00) == 0xff515700;

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

                    // Stop mouse cursor hiding & clipping
                    UpdateMouseVisible();
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

            // If mouse is currently hidden, update the clip region
            if (!mouseVisibleInternal)
                UpdateMouseClipping();
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
        if (input && !emulatedMouse)
        {
            IntVector2 center(Size() / 2);
            IntVector2 newPosition;
            IntVector2 delta;
            newPosition.x = (int)(short)LOWORD(lParam);
            newPosition.y = (int)(short)HIWORD(lParam);

            // Do not transmit mouse move when mouse should be hidden, but is not due to no input focus
            if (mouseVisibleInternal == mouseVisible)
            {
                delta = newPosition - mousePosition;
                input->OnMouseMove(newPosition, delta);
                // Recenter in hidden mouse cursor mode to allow endless relative motion
                if (!mouseVisibleInternal && delta != IntVector2::ZERO)
                    SetMousePosition(IntVector2(Width() / 2, Height() / 2));
                else
                    mousePosition = newPosition;
            }
            else
                mousePosition = newPosition;
        }
        handled = true;
        break;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (input && !emulatedMouse)
        {
            unsigned button = (msg == WM_LBUTTONDOWN) ? MOUSEB_LEFT : (msg == WM_MBUTTONDOWN) ? MOUSEB_MIDDLE : MOUSEB_RIGHT;
            input->OnMouseButton(button, true);
            // Make sure we track the button release even if mouse moves outside the window
            SetCapture((HWND)handle);
            // Re-establish mouse cursor hiding & clipping
            if (!mouseVisible && mouseVisibleInternal)
                UpdateMouseVisible();
        }
        handled = true;
        break;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (input && !emulatedMouse)
        {
            unsigned button = (msg == WM_LBUTTONUP) ? MOUSEB_LEFT : (msg == WM_MBUTTONUP) ? MOUSEB_MIDDLE : MOUSEB_RIGHT;
            input->OnMouseButton(button, false);
            // End capture when there are no more mouse buttons held down
            if (!input->MouseButtons())
                ReleaseCapture();
        }
        handled = true;
        break;

    case WM_TOUCH:
        if (input && LOWORD(wParam))
        {
            Vector<TOUCHINPUT> touches(LOWORD(wParam));
            if (getTouchInputInfo((HTOUCHINPUT)lParam, (unsigned)touches.Size(), &touches[0], sizeof(TOUCHINPUT)))
            {
                for (auto it = touches.Begin(); it != touches.End(); ++it)
                {
                    // Translate touch points inside window
                    POINT point;
                    point.x = it->x / 100;
                    point.y = it->y / 100;
                    ScreenToClient((HWND)handle, &point);
                    IntVector2 position(point.x, point.y);

                    if (it->dwFlags & (TOUCHEVENTF_DOWN || TOUCHEVENTF_UP))
                        input->OnTouch(it->dwID, true, position, 1.0f);
                    else if (it->dwFlags & TOUCHEVENTF_UP)
                        input->OnTouch(it->dwID, false, position, 1.0f);

                    // Mouse cursor will move to the position of the touch on next move, so move beforehand
                    // to prevent erratic jumps in hidden mouse mode
                    if (!mouseVisibleInternal)
                        mousePosition = position;
                }
            }
        }

        closeTouchInputHandle((HTOUCHINPUT)lParam);
        handled = true;
        break;

    case WM_SYSCOMMAND:
        // Prevent system bell sound from Alt key combinations
        if ((wParam & 0xff00) == SC_KEYMENU)
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

void Window::UpdateMouseVisible()
{
    if (!handle)
        return;

    // When the window is unfocused, mouse should never be hidden
    bool newMouseVisible = HasFocus() ? mouseVisible : true;
    if (newMouseVisible != mouseVisibleInternal)
    {
        ShowCursor(newMouseVisible ? TRUE : FALSE);
        mouseVisibleInternal = newMouseVisible;
    }

    UpdateMouseClipping();
}

void Window::UpdateMouseClipping()
{
    if (mouseVisibleInternal)
        ClipCursor(nullptr);
    else
    {
        RECT mouseRect;
        POINT point;
        IntVector2 windowSize = Size();

        point.x = point.y = 0;
        ClientToScreen((HWND)handle, &point);
        mouseRect.left = point.x;
        mouseRect.top = point.y;
        mouseRect.right = point.x + windowSize.x;
        mouseRect.bottom = point.y + windowSize.y;
        ClipCursor(&mouseRect);
    }
}

void Window::UpdateMousePosition()
{
    POINT screenPosition;
    GetCursorPos(&screenPosition);
    ScreenToClient((HWND)handle, &screenPosition);
    mousePosition.x = screenPosition.x;
    mousePosition.y = screenPosition.y;
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