#pragma once

#include "loom/core/window.h"
#include <GLFW/glfw3.h>

namespace Loom {

    class LinuxWindow : public Window {
    public:
        LinuxWindow(const WindowProps& props);
        ~LinuxWindow() override;

        void OnUpdate() override;

        unsigned int GetWidth() const override { return mData.Width; }
        unsigned int GetHeight() const override { return mData.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override { mData.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        GLFWwindow* mWindow;

        struct WindowData {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;
            EventCallbackFn EventCallback;
        };

        WindowData mData;
    };

} // namespace Loom