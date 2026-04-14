#pragma once

#include "loom/core/window.h"
#include "loom/events/application_event.h"
#include <memory>

namespace Loom {

    class LOOM_API Application {
    public:
        Application();
        virtual ~Application();

        void Run();
        void OnEvent(Event& event);

        Window& GetWindow() { return *mWindow; }
        static Application& Get() { return *sInstance; }

    private:
        bool OnWindowClose(WindowCloseEvent& event);

    private:
        std::unique_ptr<Window> mWindow;
        bool mRunning = true;

        static Application* sInstance;
    };

} // namespace Loom