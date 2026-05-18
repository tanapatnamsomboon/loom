#include <loom/renderer/editor_camera.h>
#include <loom/core/input.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace Loom {

    EditorCamera::EditorCamera(float fov, float aspect_ratio, float near_clip, float far_clip)
        : mFOV(fov), mAspectRatio(aspect_ratio), mNearClip(near_clip), mFarClip(far_clip) {
        UpdateProjection();
        UpdateView();
    }

    void EditorCamera::OnUpdate(Timestep ts) {
        glm::vec2 mouse_pos = { Input::GetMouseX(), Input::GetMouseY() };
        glm::vec2 delta     = mouse_pos - mInitialMousePosition;
        mInitialMousePosition = mouse_pos;

        bool rmb = Input::IsMouseButtonPressed(Mouse::ButtonRight);
        bool mmb = Input::IsMouseButtonPressed(Mouse::ButtonMiddle);

        // Orientation reused below for direction vectors.
        glm::quat orient  = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
        glm::vec3 forward = glm::rotate(orient, glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 right   = glm::rotate(orient, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec3 up      = glm::rotate(orient, glm::vec3(0.0f, 1.0f, 0.0f));

        if (mmb) {
            // Blender-style: MMB = orbit, Shift+MMB = pan. Pan speed scales
            // with distance so the world tracks the cursor at any zoom level.
            bool shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift);
            if (shift) {
                float pan_speed = mDistance * 0.0015f;
                mTarget -= right * (delta.x * pan_speed);
                mTarget += up    * (delta.y * pan_speed);
            } else {
                mYaw   += delta.x * 0.005f;
                mPitch += delta.y * 0.005f;
                // Clamp pitch just shy of the poles to avoid lookAt gimbal flip.
                const float pole = glm::half_pi<float>() - 0.01f;
                mPitch = std::clamp(mPitch, -pole, pole);
            }
        } else if (rmb) {
            // Unity / Unreal habit: RMB-look + WASD-fly. The target follows the
            // camera so subsequent MMB orbit still has something meaningful to
            // pivot around (whatever's `mDistance` in front of you).
            mYaw   += delta.x * 0.003f;
            mPitch += delta.y * 0.003f;
            const float pole = glm::half_pi<float>() - 0.01f;
            mPitch = std::clamp(mPitch, -pole, pole);

            // Recompute basis after pitch/yaw change so WASD this frame uses
            // the post-look direction (matches FPS-camera muscle memory).
            orient  = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
            forward = glm::rotate(orient, glm::vec3(0.0f, 0.0f, -1.0f));
            right   = glm::rotate(orient, glm::vec3(1.0f, 0.0f, 0.0f));
            up      = glm::rotate(orient, glm::vec3(0.0f, 1.0f, 0.0f));

            glm::vec3 move(0.0f);
            if (Input::IsKeyPressed(Key::W)) move += forward;
            if (Input::IsKeyPressed(Key::S)) move -= forward;
            if (Input::IsKeyPressed(Key::A)) move -= right;
            if (Input::IsKeyPressed(Key::D)) move += right;
            if (Input::IsKeyPressed(Key::Q)) move -= up;
            if (Input::IsKeyPressed(Key::E)) move += up;
            mTarget += move * mCameraSpeed * (float)ts;
        }

        UpdateView();
    }

    void EditorCamera::OnEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseScrolledEvent>(LOOM_BIND_EVENT_FN(OnMouseScroll));
    }

    void EditorCamera::SetViewportSize(float width, float height) {
        mAspectRatio = width / height;
        UpdateProjection();
    }

    void EditorCamera::UpdateProjection() {
        mAspectRatio = mAspectRatio == 0.0f ? 1.0f : mAspectRatio;
        mProjection = glm::perspective(glm::radians(mFOV), mAspectRatio, mNearClip, mFarClip);
    }

    void EditorCamera::UpdateView() {
        glm::quat orient  = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
        glm::vec3 forward = glm::rotate(orient, glm::vec3(0.0f, 0.0f, -1.0f));
        // Derive position from target / distance / orientation each frame.
        // This makes orbit, pan, dolly, and fly all compose through a single
        // path — there is no separate "camera position" state to keep in sync.
        mPosition   = mTarget - forward * mDistance;
        mViewMatrix = glm::translate(glm::mat4(1.0f), mPosition) * glm::toMat4(orient);
        mViewMatrix = glm::inverse(mViewMatrix);
    }

    void EditorCamera::SetState(const glm::vec3& position, float pitch, float yaw) {
        // Restore from the legacy save format (position + pitch + yaw). Derive
        // the orbit target as a sensible default 5 units in front of the
        // restored pose so MMB orbit lands on plausible geometry; the user can
        // re-frame anything by pressing F.
        mPitch    = pitch;
        mYaw      = yaw;
        mDistance = 5.0f;
        glm::quat orient  = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
        glm::vec3 forward = glm::rotate(orient, glm::vec3(0.0f, 0.0f, -1.0f));
        mTarget   = position + forward * mDistance;
        UpdateView();
    }

    void EditorCamera::FocusOn(const glm::vec3& world_target, float fit_radius) {
        mTarget   = world_target;
        // Distance such that `fit_radius` exactly fills the vertical FOV.
        float half_fov = glm::radians(mFOV) * 0.5f;
        mDistance = std::max(fit_radius / std::sin(half_fov), 0.5f);
        UpdateView();
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& event) {
        // Multiplicative dolly toward / away from target. exp() gives the same
        // proportional step at any zoom level — fine control close, coarse far.
        // Scroll-up = closer (negative exponent), scroll-down = farther.
        mDistance *= std::exp(-event.GetYOffset() * 0.15f);
        mDistance  = std::clamp(mDistance, 0.1f, 1000.0f);
        UpdateView();
        return false;
    }

} // namespace Loom
