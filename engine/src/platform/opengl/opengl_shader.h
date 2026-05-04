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
        void UploadUniformFloat(const std::string& name, float value) override;
        void UploadUniformFloat4(const std::string& name, const glm::vec4& values) override;
        void UploadUniformFloat3(const std::string& name, const glm::vec3& values) override;
        void UploadUniformInt(const std::string& name, int value) override;
        void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) override;

        void Reload() override;

    private:
        std::string ReadFile(const std::string& filepath);
        void Compile(const std::string& vertex_src, const std::string& fragment_src);

    private:
        uint32_t    mRendererID = 0;
        std::string mFilePath;   // base path (no extension); empty for src-string shaders
    };

} // namespace Loom