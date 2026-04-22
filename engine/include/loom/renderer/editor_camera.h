#pragma once

#include <loom/core/timestep.h>
#include <loom/core/input.h>
#include <loom/events/event.h>
#include <loom/events/mouse_event.h>
#include <loom/renderer/camera.h>
#include <glm/glm.hpp>

namespace Loom {

    class LOOM_API EditorCamera : public Camera {
    public:
        EditorCamera() = default;
        EditorCamera(float fov, float aspect_ratio, float near_clip, float far_clip);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& event);

        void SetViewportSize(float width, float height);

        const glm::mat4& GetViewMatrix() const { return mViewMatrix; }
        glm::mat4 GetViewProjectionMatrix() const { return mProjection * mViewMatrix; }

        const glm::vec3& GetPosition() const { return mPosition; }

        void ResetMousePosition() { mInitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() }; }

    private:
        void UpdateProjection();
        void UpdateView();
        bool OnMouseScroll(MouseScrolledEvent& event);

    private:
        float mFOV = 45.0f, mAspectRatio = 1.778f, mNearClip = 0.1f, mFarClip = 1000.0f;
        glm::mat4 mViewMatrix = glm::mat4(1.0f);
        glm::vec3 mPosition = { 0.0f, 0.0f, 5.0f };

        float mPitch = 0.0f, mYaw = 0.0f;
        glm::vec2 mInitialMousePosition = { 0.0f, 0.0f };
        float mCameraSpeed = 5.0f;
    };

} // namespace Loom