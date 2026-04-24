#pragma once

#include "loom/core/core.h"
#include "loom/renderer/vertex_array.h"

namespace Loom {

    class LOOM_API RendererAPI {
    public:
        enum class API {
            None = 0, OpenGL = 1
        };

        virtual ~RendererAPI() = default;

        virtual void Init() = 0;

        virtual void SetClearColor(float r, float g, float b, float a) = 0;
        virtual void Clear() = 0;

        virtual void DrawIndexed(VertexArray* vertex_array, uint32_t index_count = 0) = 0;
        virtual void DrawLines(VertexArray* vertex_array, uint32_t vertex_count) = 0;

        static API GetAPI() { return sAPI; }

    private:
        static API sAPI;
    };

} // namespace Loom