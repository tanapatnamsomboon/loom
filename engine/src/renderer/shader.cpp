#include "loom/renderer/shader.h"
#include "loom/renderer/renderer_api.h"
#include "platform/opengl/opengl_shader.h"

namespace Loom {
    std::shared_ptr<Shader> Shader::Create(const std::string& vertex_src, const std::string& fragment_src) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL: return std::make_shared<OpenGLShader>(vertex_src, fragment_src);
            default: return nullptr;
        }
    }

    std::shared_ptr<Shader> Shader::Create(const std::string& filepath) {
        switch (RendererAPI::GetAPI()) {
            case RendererAPI::API::OpenGL: return std::make_shared<OpenGLShader>(filepath);
            default: return nullptr;
        }
    }

} // namespace Loom