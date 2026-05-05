#pragma once

#include <glm/glm.hpp>
#include <string>
#include <variant>

namespace Loom {

    enum class ScriptFieldType {
        Float  = 0,
        Int    = 1,
        Bool   = 2,
        Vec2   = 3,
        Vec3   = 4,
        String = 5
    };

    struct ScriptField {
        std::string     Name;
        ScriptFieldType Type  = ScriptFieldType::Float;
        std::variant<float, int, bool, glm::vec2, glm::vec3, std::string> Value;
    };

} // namespace Loom
