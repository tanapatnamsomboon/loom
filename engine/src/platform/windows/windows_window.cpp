#include "platform/windows/windows_window.h"
#include "loom/core/log.h"
#include "loom/events/application_event.h"

namespace Loom {

    LRESULT CALLBACK LoomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        WindowsWindow::WindowData* pData = (WindowsWindow::WindowData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg) {
            case WM_CLOSE: {
                if (pData) {
                    WindowCloseEvent event;
                    pData->EventCallback(event);
                }
                return 0;
            }
            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
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

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = LoomWndProc;
        wc.hInstance = mHInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"LoomEngineWindowClass";
        RegisterClassExW(&wc);

        int size_needed = MultiByteToWideChar(CP_UTF8, 0, mData.Title.c_str(), (int)mData.Title.size(), nullptr, 0);
        std::wstring wide_title(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, mData.Title.c_str(), (int)mData.Title.size(), &wide_title[0], size_needed);

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