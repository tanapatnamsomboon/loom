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

    Ray ScreenToRay(const glm::vec2& mouse_viewport_px,
                    const glm::vec2& viewport_size,
                    const glm::mat4& view,
                    const glm::mat4& projection) {
        // Convert to NDC (-1..1). Pixel origin is top-left, NDC origin is bottom-left → flip Y.
        glm::vec2 ndc;
        ndc.x = (mouse_viewport_px.x / viewport_size.x) * 2.0f - 1.0f;
        ndc.y = 1.0f - (mouse_viewport_px.y / viewport_size.y) * 2.0f;

        glm::mat4 inv_vp = glm::inverse(projection * view);

        glm::vec4 near_h = inv_vp * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
        glm::vec4 far_h  = inv_vp * glm::vec4(ndc.x, ndc.y,  1.0f, 1.0f);
        glm::vec3 near_w = glm::vec3(near_h) / near_h.w;
        glm::vec3 far_w  = glm::vec3(far_h)  / far_h.w;

        Ray r;
        r.Origin    = near_w;
        r.Direction = glm::normalize(far_w - near_w);
        return r;
    }

    std::optional<glm::vec2> WorldToScreen(const glm::vec3& world_pos,
                                          const glm::vec2& viewport_size,
                                          const glm::mat4& view_projection) {
        glm::vec4 clip = view_projection * glm::vec4(world_pos, 1.0f);
        if (clip.w <= 0.0f)
            return std::nullopt;

        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        glm::vec2 screen;
        screen.x = (ndc.x * 0.5f + 0.5f) * viewport_size.x;
        screen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * viewport_size.y;
        return screen;
    }

    std::optional<glm::vec3> RayPlaneIntersect(const Ray& ray,
                                               const glm::vec3& plane_point,
                                               const glm::vec3& plane_normal) {
        float denom = glm::dot(ray.Direction, plane_normal);
        if (glm::abs(denom) < 1e-6f)
            return std::nullopt; // parallel

        float t = glm::dot(plane_point - ray.Origin, plane_normal) / denom;
        if (t < 0.0f)
            return std::nullopt; // behind ray origin

        return ray.At(t);
    }

    glm::vec3 ClosestPointOnLine(const glm::vec3& line_point,
                                 const glm::vec3& line_dir,
                                 const glm::vec3& point) {
        float t = glm::dot(point - line_point, line_dir);
        return line_point + line_dir * t;
    }

    float DistancePointToSegment2D(const glm::vec2& p,
                                   const glm::vec2& a,
                                   const glm::vec2& b) {
        glm::vec2 ab = b - a;
        float len_sq = glm::dot(ab, ab);
        if (len_sq < 1e-6f)
            return glm::length(p - a);

        float t = glm::clamp(glm::dot(p - a, ab) / len_sq, 0.0f, 1.0f);
        glm::vec2 proj = a + ab * t;
        return glm::length(p - proj);
    }

} // namespace Loom
