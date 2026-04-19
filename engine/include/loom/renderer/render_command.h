#pragma once

#include "loom/core/core.h"
#include "loom/renderer/renderer_api.h"

namespace Loom {

    class LOOM_API RenderCommand {
    public:
        static void Init() {
            sRendererAPI->Init();
        }

        static void SetClearColor(float r, float g, float b, float a) {
            sRendererAPI->SetClearColor(r, g, b, a);
        }

        static void Clear() {
            sRendererAPI->Clear();
        }

        static void DrawIndexed(VertexArray* vertex_array, uint32_t index_count = 0) {
            sRendererAPI->DrawIndexed(vertex_array, index_count);
        }

    private:
        static RendererAPI* sRendererAPI;
    };

} // namespace Loom