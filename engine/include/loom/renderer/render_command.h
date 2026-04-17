#pragma once

#include "loom/core/core.h"
#include "loom/renderer/renderer_api.h"

namespace Loom {

    class LOOM_API RenderCommand {
    public:
        static void SetClearColor(float r, float g, float b, float a) {
            sRendererAPI->SetClearColor(r, g, b, a);
        }

        static void Clear() {
            sRendererAPI->Clear();
        }

        static void DrawIndexed(VertexArray* vertex_array) {
            sRendererAPI->DrawIndexed(vertex_array);
        }

    private:
        static RendererAPI* sRendererAPI;
    };

} // namespace Loom