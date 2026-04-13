#include "loom/core/application.h"
#include "loom/core/log.h"
#include <iostream>

namespace Loom {
    Application::Application() {}
    Application::~Application() {}

    void Application::Run() {
        LOOM_CORE_INFO("Engine is running!");
        LOOM_CORE_WARN("This is a core warning test.");
    }
} // namespace Loom