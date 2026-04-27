#pragma once

#include "loom/core/window.h"
#include "loom/renderer/graphics_context.h"
#include <Windows.h>

namespace Loom {

    class WindowsWindow : public Window {
    public:
        WindowsWindow(const WindowProps& props);
        ~WindowsWindow() override;

        void OnUpdate() override;

        unsigned int GetWidth() const override { return mData.Width; }
        unsigned int GetHeight() const override { return mData.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override { mData.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;
        void* GetNativeWindow() const override { return mHWND; }

        struct WindowData {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;
            EventCallbackFn EventCallback;
        };

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        HWND mHWND;
        HINSTANCE mHInstance;
        std::unique_ptr<GraphicsContext> mContext;
        WindowData mData;
    };

} // namespace Loom