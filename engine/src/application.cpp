#include "loom/application.h"
#include <iostream>

namespace Loom {
    Application::Application() {}
    Application::~Application() {}

    void Application::Run() {
        std::cout << "Engine is successfully running and linked!" << std::endl;
    }
} // namespace Loom