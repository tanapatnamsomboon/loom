#pragma once

#include "loom/core/core.h"
#include <string>
#include <glm/glm.hpp>
#include <memory>

namespace Loom {

    class LOOM_API Shader {
    public:
        virtual ~Shader() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;
        virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& values) = 0;
        virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& values) = 0;
        virtual void UploadUniformInt(const std::string& name, int value) = 0;
        virtual void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) = 0;

        static std::shared_ptr<Shader> Create(const std::string& vertex_src, const std::string& fragment_src);
        static std::shared_ptr<Shader> Create(const std::string& filepath);
    };

} // namespace Loom