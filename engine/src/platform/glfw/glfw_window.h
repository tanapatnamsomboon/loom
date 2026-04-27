#pragma once

#include "loom/core/window.h"
#include "loom/renderer/graphics_context.h"
#include <GLFW/glfw3.h>

namespace Loom {

    class GLFWWindow : public Window {
    public:
        GLFWWindow(const WindowProps& props);
        ~GLFWWindow() override;

        void OnUpdate() override;

        unsigned int GetWidth() const override { return mData.Width; }
        unsigned int GetHeight() const override { return mData.Height; }

        void SetEventCallback(const EventCallbackFn& callback) override { mData.EventCallback = callback; }
        void SetVSync(bool enabled) override;
        bool IsVSync() const override;

        void* GetNativeWindow() const override { return mWindow; }

    private:
        void Init(const WindowProps& props);
        void Shutdown();

    private:
        GLFWwindow* mWindow;
        std::unique_ptr<GraphicsContext> mContext;

        struct WindowData {
            std::string Title;
            unsigned int Width, Height;
            bool VSync;
            EventCallbackFn EventCallback;
        };

        WindowData mData;
    };

} // namespace Loom