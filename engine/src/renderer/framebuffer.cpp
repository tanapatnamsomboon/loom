#include "loom/renderer/framebuffer.h"
#include "loom/renderer/renderer_api.h"
#include "platform/opengl/opengl_framebuffer.h"

namespace Loom {

    std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL: return std::make_shared<OpenGLFramebuffer>(spec);
            default: return nullptr;
        }
    }

} // namespace Loom