#pragma once

#include "loom/core/core.h"
#include <glm/glm.hpp>
#include <optional>

namespace Loom::Math {

    bool LOOM_API DecomposeTransform(const glm::mat4& transform, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale);

    // Ray in world space. Direction is unit-length.
    struct Ray {
        glm::vec3 Origin    = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Direction = { 0.0f, 0.0f, -1.0f };

        glm::vec3 At(float t) const { return Origin + Direction * t; }
    };

    // Builds a world-space ray from a viewport-relative mouse position (pixels, origin at top-left).
    // viewport_size is in pixels. Returns the ray from the camera through that pixel.
    Ray LOOM_API ScreenToRay(const glm::vec2& mouse_viewport_px,
                             const glm::vec2& viewport_size,
                             const glm::mat4& view,
                             const glm::mat4& projection);

    // Projects a world-space point to viewport-relative pixel coordinates (origin at top-left).
    // Returns std::nullopt if the point is behind the camera (w <= 0).
    std::optional<glm::vec2> LOOM_API WorldToScreen(const glm::vec3& world_pos,
                                                   const glm::vec2& viewport_size,
                                                   const glm::mat4& view_projection);

    // Ray–plane intersection. Returns std::nullopt if the ray is parallel to the plane or hits behind the origin.
    // plane_normal is expected to be unit-length.
    std::optional<glm::vec3> LOOM_API RayPlaneIntersect(const Ray& ray,
                                                      const glm::vec3& plane_point,
                                                      const glm::vec3& plane_normal);

    // Closest point on the infinite line through line_point along line_dir (unit-length) to `point`.
    // Used for axis-drag projection: snap a free 3D point back to the gizmo axis it was dragged along.
    glm::vec3 LOOM_API ClosestPointOnLine(const glm::vec3& line_point,
                                          const glm::vec3& line_dir,
                                          const glm::vec3& point);

    // Shortest distance (pixels) from a screen-space point to the segment [a, b].
    // Used for screen-space proximity picking of gizmo axis handles.
    float LOOM_API DistancePointToSegment2D(const glm::vec2& p,
                                            const glm::vec2& a,
                                            const glm::vec2& b);

} // namespace Loom
