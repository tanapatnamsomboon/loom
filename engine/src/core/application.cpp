#include "loom/core/application.h"
#include "loom/core/log.h"

namespace Loom {

    Application::Application() {
        mWindow = std::unique_ptr<Window>(Window::Create());
        mWindow->SetEventCallback(LOOM_BIND_EVENT_FN(Application::OnEvent));
    }

    Application::~Application() {}

    void Application::Run() {
        while (mRunning) {
            mWindow->OnUpdate();
        }
    }

    void Application::OnEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(LOOM_BIND_EVENT_FN(Application::OnWindowClose));

        LOOM_CORE_TRACE("{0}", event.ToString());
    }

    bool Application::OnWindowClose(WindowCloseEvent& event) {
        mRunning = false;
        return true;
    }
} // namespace Loom