#pragma once

#include "loom/renderer/renderer_api.h"

namespace Loom {

    class OpenGLRendererAPI : public RendererAPI {
    public:
        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;
    };

} // namespace Loom