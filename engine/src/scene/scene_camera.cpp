#include "loom/scene/scene_camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Loom {

    SceneCamera::SceneCamera() {
        RecalculateProjection();
    }

    void SceneCamera::SetPerspective(float vertical_fov, float near_clip, float far_clip) {
        mProjectionType = ProjectionType::Perspective;
        mPerspectiveFOV = vertical_fov;
        mPerspectiveNear = near_clip;
        mPerspectiveFar = far_clip;
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographic(float size, float near_clip, float far_clip) {
        mProjectionType = ProjectionType::Orthographic;
        mOrthographicSize = size;
        mOrthographicNear = near_clip;
        mOrthographicFar = far_clip;
        RecalculateProjection();
    }

    void SceneCamera::SetViewportSize(uint32_t width, uint32_t height) {
        if (height == 0) return;
        mAspectRatio = (float)width / (float)height;
        RecalculateProjection();
    }

    void SceneCamera::RecalculateProjection() {
        if (mProjectionType == ProjectionType::Perspective) {
            mProjection = glm::perspective(mPerspectiveFOV, mAspectRatio, mPerspectiveNear, mPerspectiveFar);
        } else {
            float ortho_left = -mOrthographicSize * mAspectRatio * 0.5f;
            float ortho_right = mOrthographicSize * mAspectRatio * 0.5f;
            float ortho_bottom = -mOrthographicSize * 0.5f;
            float ortho_top = mOrthographicSize * 0.5f;

            mProjection = glm::ortho(ortho_left, ortho_right, ortho_bottom, ortho_top, mOrthographicNear, mOrthographicFar);
        }
    }

} // namespace Loom