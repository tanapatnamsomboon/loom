#include "loom/math/math.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace Loom::Math {

    bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale) {
        using namespace glm;
        using T = float;

        mat4 LocalMatrix(transform);

        if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>()))
            return false;

        if (epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>())) {
            LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
            LocalMatrix[3][3] = static_cast<T>(1);
        }

        translation = vec3(LocalMatrix[3]);
        LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

        vec3 Row[3], Pdum3;

#if 0
        Pdum3 = cross(Row[1], Row[2]);
        if (dot(Row[0], Pdum3) < 0) {
            for (length_t i = 0; i < 3; i++) {
                scale[i] *= static_cast<T>(-1);
                Row[i] *= static_cast<T>(-1);
            }
        }
#endif

        for (length_t i = 0; i < 3; ++i)
            for (length_t j = 0; j < 3; ++j)
                    Row[i][j] = LocalMatrix[i][j];

        scale.x = length(Row[0]);
        Row[0] = detail::scale(Row[0], static_cast<T>(1));
        scale.y = length(Row[1]);
        Row[1] = detail::scale(Row[1], static_cast<T>(1));
        scale.z = length(Row[2]);
        Row[2] = detail::scale(Row[2], static_cast<T>(1));

        rotation.y = asin(-Row[0][2]);
        if (cos(rotation.y) != 0) {
            rotation.x = atan2(Row[1][2], Row[2][2]);
            rotation.z = atan2(Row[0][1], Row[0][0]);
        } else {
            rotation.x = atan2(-Row[2][0], Row[1][1]);
            rotation.z = 0;
        }

        return true;
    }

} // namespace Loom