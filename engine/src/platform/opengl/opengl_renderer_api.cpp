#include "platform/opengl/opengl_renderer_api.h"
#include <glad/glad.h>

namespace Loom {

    void OpenGLRendererAPI::Init() {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void OpenGLRendererAPI::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(VertexArray* vertex_array, uint32_t index_count) {
        vertex_array->Bind();
        uint32_t count = index_count ? index_count : vertex_array->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::DrawLines(VertexArray* vertex_array, uint32_t vertex_count) {
        vertex_array->Bind();
        glDrawArrays(GL_LINES, 0, vertex_count);
    }

} // namespace Loom
