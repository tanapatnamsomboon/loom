#pragma once

#include "loom/core.h"

namespace Loom {

    class ENGINE_API Application {
    public:
        Application();
        virtual ~Application();
        void Run();
    };

} // namespace Loom