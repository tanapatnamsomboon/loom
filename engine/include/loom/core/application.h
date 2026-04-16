#pragma once

#include "loom/core/layer_stack.h"
#include "loom/core/window.h"
#include "loom/events/application_event.h"
#include "loom/imgui/imgui_layer.h"
#include <memory>

namespace Loom {

    class LOOM_API Application {
    public:
        Application();
        virtual ~Application();

        void Run();
        void OnEvent(Event& event);

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        Window& GetWindow() { return *mWindow; }
        static Application& Get() { return *sInstance; }

    private:
        bool OnWindowClose(WindowCloseEvent& event);

    private:
        std::unique_ptr<Window> mWindow;
        bool mRunning = true;

        LayerStack mLayerStack;
        ImGuiLayer* mImGuiLayer;

        static Application* sInstance;
    };

} // namespace Loom