#pragma once

#include "loom/core/window.h"
#include <memory>

namespace Loom {

    class ENGINE_API Application {
    public:
        Application();
        virtual ~Application();
        void Run();

    private:
        std::unique_ptr<Window> mWindow;
        bool mRunning = true;
    };

} // namespace Loom