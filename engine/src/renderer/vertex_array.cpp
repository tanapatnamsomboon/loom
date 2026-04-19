#include "loom/renderer/vertex_array.h"
#include "loom/renderer/renderer_api.h"
#include "platform/opengl/opengl_vertex_array.h"

namespace Loom {
    std::shared_ptr<VertexArray> VertexArray::Create() {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLVertexArray>();
            default: return nullptr;
        }
    }

} // namespace Loom