#pragma once

#include "loom/core/core.h"
#include <string>
#include <glm/glm.hpp>

namespace Loom {

    class LOOM_API Shader {
    public:
        virtual ~Shader() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;
        virtual void UploadUniformInt(const std::string& name, int value) = 0;

        static Shader* Create(const std::string& vertex_src, const std::string& fragment_src);
    };

} // namespace Loom