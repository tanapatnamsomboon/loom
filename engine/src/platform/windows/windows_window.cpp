#include "platform/windows/windows_window.h"
#include "loom/core/log.h"
#include "loom/events/application_event.h"
#include "loom/events/key_event.h"
#include "loom/events/mouse_event.h"
#include <Windowsx.h>

namespace Loom {

    static std::wstring StringToWideString(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
        std::wstring out(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)out.size(), &out[0], size_needed);
        return out;
    }

    LRESULT CALLBACK LoomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowsWindow::WindowData* data = (WindowsWindow::WindowData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg) {
            case WM_DESTROY: {
                PostQuitMessage(0);
                break;
            }
            case WM_CLOSE: {
                if (data) {
                    WindowCloseEvent event;
                    data->EventCallback(event);
                }
                return 0;
            }
            case WM_SIZE: {
                if (data) {
                    UINT width = LOWORD(lParam);
                    UINT height = HIWORD(lParam);
                    WindowResizeEvent event(width, height);
                    data->EventCallback(event);
                }
                return 0;
            }
            case WM_SETFOCUS: {
                if (data) {
                }
                break;
            }
            case WM_KILLFOCUS: {
                if (data) {
                }
                break;
            }
            case WM_MOVE: {
                if (data) {
                }
                break;
            }
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                if (data) {
                    int keycode = (int)wParam;
                    int repeat_count = lParam & 0xFFFF;
                    KeyPressedEvent event(keycode, repeat_count);
                    data->EventCallback(event);
                }
                break;
            }
            case WM_KEYUP:
            case WM_SYSKEYUP: {
                if (data) {
                    int keycode = (int)wParam;
                    KeyReleasedEvent event(keycode);
                    data->EventCallback(event);
                }
                break;
            }
            case WM_CHAR: {
                if (data) {
                }
                break;
            }
            case WM_LBUTTONDOWN: {
                if (data) {
                }
                break;
            }
            case WM_MBUTTONDOWN: {
                if (data) {
                }
                break;
            }
            case WM_RBUTTONDOWN: {
                if (data) {
                }
                break;
            }
            case WM_LBUTTONUP: {
                if (data) {
                }
                break;
            }
            case WM_MBUTTONUP: {
                if (data) {
                }
                break;
            }
            case WM_RBUTTONUP: {
                if (data) {
                }
                break;
            }
            case WM_MOUSEMOVE: {
                if (data) {
                    int xpos = GET_X_LPARAM(lParam);
                    int ypos = GET_Y_LPARAM(lParam);
                    MouseMovedEvent event(xpos, ypos);
                    data->EventCallback(event);
                }
                break;
            }
            case WM_MOUSEWHEEL: {
                if (data) {
                    int xoffset = GET_X_LPARAM(lParam);
                    int yoffset = GET_Y_LPARAM(lParam);
                    MouseScrolledEvent event(xoffset, yoffset);
                    data->EventCallback(event);
                }
                break;
            }
            default: break;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    WindowsWindow::WindowsWindow(const WindowProps& props) {
        Init(props);
    }

    WindowsWindow::~WindowsWindow() {
        Shutdown();
    }

    void WindowsWindow::OnUpdate() {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void WindowsWindow::SetVSync(bool enabled) {
        mData.VSync = enabled;
    }

    bool WindowsWindow::IsVSync() const {
        return mData.VSync;
    }

    void WindowsWindow::Init(const WindowProps& props) {
        mData.Title  = props.Title;
        mData.Width  = props.Width;
        mData.Height = props.Height;
        mHInstance = GetModuleHandleW(nullptr);

        LOOM_CORE_INFO("Creating Win32 window {0} ({1}, {2})", props.Title, props.Width, props.Height);

        std::wstring wide_title = StringToWideString(mData.Title);
        LPCWSTR class_name = L"LoomEngineWindowClass";

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = LoomWndProc;
        wc.hInstance = mHInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = class_name;
        RegisterClassExW(&wc);

        mHWND = CreateWindowExW(
            0, L"LoomEngineWindowClass", wide_title.c_str(),
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, mData.Width, mData.Height,
            nullptr, nullptr, mHInstance, nullptr
        );

        SetWindowLongPtrW(mHWND, GWLP_USERDATA, (LONG_PTR)&mData);
    }

    void WindowsWindow::Shutdown() {
        DestroyWindow(mHWND);
        UnregisterClassW(L"LoomEngineWindowClass", mHInstance);
    }

} // namespace Loom