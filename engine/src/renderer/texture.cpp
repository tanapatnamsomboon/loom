#include "loom/renderer/texture.h"
#include "loom/renderer/renderer_api.h"
#include "platform/opengl/opengl_texture.h"

namespace Loom {
    std::shared_ptr<Texture2D> Texture2D::Create(const std::string& path) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLTexture2D>(path);
            default: return nullptr;
        }
    }

    std::shared_ptr<Texture2D> Texture2D::Create(uint32_t width, uint32_t height) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL:  return std::make_shared<OpenGLTexture2D>(width, height);
            default: return nullptr;
        }
    }

} // namespace Loom