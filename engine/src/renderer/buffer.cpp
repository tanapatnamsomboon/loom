#include "loom/renderer/buffer.h"
#include "loom/renderer/renderer_api.h"
#include "loom/core/log.h"
#include "platform/opengl/opengl_buffer.h"

namespace Loom {

    VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::None:    LOOM_CORE_FATAL("RendererAPI::None is currently not supported!"); return nullptr;
            case RendererAPI::API::OpenGL:  return new OpenGLVertexBuffer(vertices, size);
        }
        LOOM_CORE_FATAL("Unknown RendererAPI!");
        return nullptr;
    }

    IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::None:    LOOM_CORE_FATAL("RendererAPI::None is currently not supported!"); return nullptr;
            case RendererAPI::API::OpenGL:  return new OpenGLIndexBuffer(indices, count);
        }
        LOOM_CORE_FATAL("Unknown RendererAPI!");
        return nullptr;
    }

} // namespace Loom