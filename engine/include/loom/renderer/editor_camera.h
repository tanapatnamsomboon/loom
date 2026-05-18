#pragma once

#include <loom/core/timestep.h>
#include <loom/core/input.h>
#include <loom/events/event.h>
#include <loom/events/mouse_event.h>
#include <loom/renderer/camera.h>
#include <glm/glm.hpp>

namespace Loom {

    // Blender-style editor camera with a target / pivot point.
    //
    // Controls:
    //   MMB drag             orbit around target
    //   Shift + MMB drag     pan (translates target in screen space)
    //   Scroll               dolly toward/away from target (multiplicative)
    //   RMB drag + WASD/QE   FPS-style free-fly (Unity/Unreal habit), target follows
    //
    // Internal state is (target, distance, pitch, yaw); position is derived as
    // `target - forward * distance` each frame, so all controls compose cleanly.
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
        float GetPitch() const { return mPitch; }
        float GetYaw()   const { return mYaw; }

        // Restores camera state and rebuilds the view matrix. Sets the orbit
        // target to a sensible default in front of the restored position so
        // MMB orbit still works against the loaded camera pose.
        void SetState(const glm::vec3& position, float pitch, float yaw);

        // Frames a world-space point at `fit_radius` world units of clearance.
        // Used by the "F to focus selected" shortcut in EditorLayer.
        void FocusOn(const glm::vec3& world_target, float fit_radius);

        void ResetMousePosition() { mInitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() }; }

        float GetCameraSpeed() const { return mCameraSpeed; }
        void SetCameraSpeed(float speed) { mCameraSpeed = speed; }

    private:
        void UpdateProjection();
        void UpdateView();
        bool OnMouseScroll(MouseScrolledEvent& event);

    private:
        float mFOV = 45.0f, mAspectRatio = 1.778f, mNearClip = 0.1f, mFarClip = 1000.0f;
        glm::mat4 mViewMatrix = glm::mat4(1.0f);

        // Orbit pivot + distance — the authoritative state. Position is derived.
        glm::vec3 mTarget   = { 0.0f, 0.0f, 0.0f };
        float     mDistance = 5.0f;
        float     mPitch    = 0.0f;
        float     mYaw      = 0.0f;

        // Cached every UpdateView() — readable via GetPosition() for shaders + grids.
        glm::vec3 mPosition = { 0.0f, 0.0f, 5.0f };

        glm::vec2 mInitialMousePosition = { 0.0f, 0.0f };
        float mCameraSpeed = 5.0f;
    };

} // namespace Loom