#include "loom/core/application.h"
#include "loom/core/log.h"

namespace Loom {

    Application::Application() {
        mWindow = std::unique_ptr<Window>(Window::Create());
    }

    Application::~Application() {}

    void Application::Run() {
        while (mRunning) {
            mWindow->OnUpdate();
        }
    }

} // namespace Loom