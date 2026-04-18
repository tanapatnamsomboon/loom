#include "loom/renderer/buffer.h"
#include "loom/renderer/renderer_api.h"
#include "loom/core/log.h"
#include "platform/opengl/opengl_buffer.h"

namespace Loom {
    std::shared_ptr<VertexBuffer> VertexBuffer::Create(uint32_t size) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::None:    LOOM_CORE_FATAL("RendererAPI::None is currently not supported!"); return nullptr;
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLVertexBuffer>(size);
        }
        LOOM_CORE_FATAL("Unknown RendererAPI!");
        return nullptr;
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::None:    LOOM_CORE_FATAL("RendererAPI::None is currently not supported!"); return nullptr;
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLIndexBuffer>(indices, count);
        }
        LOOM_CORE_FATAL("Unknown RendererAPI!");
        return nullptr;
    }

} // namespace Loom