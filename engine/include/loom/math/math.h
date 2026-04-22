#pragma once

#include "loom/core/core.h"
#include <glm/glm.hpp>

namespace Loom::Math {

    bool LOOM_API DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);

} // namespace Loom