#pragma once

#include "loom/renderer/shader.h"
#include <cstdint>

namespace Loom {

    class OpenGLShader : public Shader {
    public:
        OpenGLShader(const std::string& vertex_src, const std::string& fragment_src);
        ~OpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;

    private:
        uint32_t mRendererID;
    };

} // namespace Loom