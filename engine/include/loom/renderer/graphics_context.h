#pragma once
#include "loom/core/core.h"

namespace Loom {

    class LOOM_API GraphicsContext {
    public:
        virtual ~GraphicsContext() = default;

        virtual void Init() = 0;

        virtual void SwapBuffers() = 0;
    };

} // namespace Loom