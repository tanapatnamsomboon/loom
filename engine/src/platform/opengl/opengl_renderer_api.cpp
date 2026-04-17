#include "platform/opengl/opengl_renderer_api.h"
#include <glad/glad.h>

namespace Loom {

    void OpenGLRendererAPI::SetClearColor(float r, float g, float b, float a) {
        glClearColor(r, g, b, a);
    }

    void OpenGLRendererAPI::Clear() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(VertexArray* vertex_array) {
        glDrawElements(GL_TRIANGLES, vertex_array->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
    }

} // namespace Loom