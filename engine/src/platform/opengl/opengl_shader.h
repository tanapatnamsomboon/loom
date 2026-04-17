#pragma once

#include "loom/renderer/shader.h"
#include <cstdint>

namespace Loom {

    class OpenGLShader : public Shader {
    public:
        OpenGLShader(const std::string& vertex_src, const std::string& fragment_src);
        OpenGLShader(const std::string& filepath);
        ~OpenGLShader() override;

        void Bind() const override;
        void Unbind() const override;

        void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;
        void UploadUniformFloat4(const std::string& name, const glm::vec4& values) override;
        void UploadUniformFloat3(const std::string& name, const glm::vec3& values) override;
        void UploadUniformInt(const std::string& name, int value) override;

    private:
        std::string ReadFile(const std::string& filepath);
        void Compile(const std::string& vertex_src, const std::string& fragment_src);

    private:
        uint32_t mRendererID;
    };

} // namespace Loom