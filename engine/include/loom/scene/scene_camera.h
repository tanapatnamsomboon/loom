#pragma once

#include "loom/core/core.h"
#include "loom/renderer/camera.h"

namespace Loom {

    class LOOM_API SceneCamera : public Camera {
    public:
        enum class ProjectionType {
            Perspective  = 0,
            Orthographic = 1
        };

    public:
        SceneCamera();
        ~SceneCamera() override = default;

        void SetPerspective(float vertical_fov, float near_clip, float far_clip);
        void SetOrthographic(float size, float near_clip, float far_clip);

        void SetViewportSize(uint32_t width, uint32_t height);

        ProjectionType GetProjectionType() const { return mProjectionType; }
        void           SetProjectionType(ProjectionType type) {
            mProjectionType = type;
            RecalculateProjection();
        }

        float GetOrthographicSize() const { return mOrthographicSize; }
        void  SetOrthographicSize(float size) {
            mOrthographicSize = size;
            RecalculateProjection();
        }
        float GetOrthographicNearClip() const { return mOrthographicNear; }
        float GetOrthographicFarClip() const { return mOrthographicFar; }

        float GetPerspectiveVerticalFOV() const { return mPerspectiveFOV; }
        void  SetPerspectiveVerticalFOV(float vertical_fov) {
            mPerspectiveFOV = vertical_fov;
            RecalculateProjection();
        }
        float GetPerspectiveNearClip() const { return mPerspectiveNear; }
        float GetPerspectiveFarClip() const { return mPerspectiveFar; }

        float GetAspectRatio() const { return mAspectRatio; }

    private:
        void RecalculateProjection();

    private:
        ProjectionType mProjectionType = ProjectionType::Orthographic;

        float mPerspectiveFOV  = 0.785398f; // ~45 degrees in radians
        float mPerspectiveNear = 0.01f;
        float mPerspectiveFar  = 1000.0f;

        float mOrthographicSize = 10.0f;
        float mOrthographicNear = -1.0f;
        float mOrthographicFar  = 1.0f;

        float mAspectRatio = 0.0f;
    };

} // namespace Loom
