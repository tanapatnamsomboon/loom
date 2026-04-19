#pragma once

#include "loom/renderer/renderer_api.h"

namespace Loom {

    class OpenGLRendererAPI : public RendererAPI {
    public:
        void Init() override;
        void SetClearColor(float r, float g, float b, float a) override;
        void Clear() override;
        void DrawIndexed(VertexArray* vertex_array, uint32_t index_count) override;
    };

} // namespace Loom