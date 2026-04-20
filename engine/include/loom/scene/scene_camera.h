#pragma once

#include "loom/core/core.h"
#include "loom/renderer/camera.h"

namespace Loom {

    class LOOM_API SceneCamera : public Camera {
    public:
        SceneCamera();
        ~SceneCamera() override = default;

        void SetOrthographic(float size, float near_clip, float far_clip);
        void SetViewportSize(uint32_t width, uint32_t height);

        float GetOrthographicSize() const { return mOrthographicSize; }
        void SetOrthographicSize(float size) { mOrthographicSize = size; RecalculateProjection(); }

    private:
        void RecalculateProjection();

    private:
        float mOrthographicSize = 10.0f;
        float mOrthographicNear = -1.0f;
        float mOrthographicFar  = 1.0f;
        float mAspectRatio      = 0.0f;
    };

} // namespace Loom