#include <loom/renderer/editor_camera.h>
#include <loom/core/input.h>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace Loom {

    EditorCamera::EditorCamera(float fov, float aspect_ratio, float near_clip, float far_clip)
        : mFOV(fov), mAspectRatio(aspect_ratio), mNearClip(near_clip), mFarClip(far_clip) {
        UpdateProjection();
        UpdateView();
    }

    void EditorCamera::OnUpdate(Timestep ts) {
        if (Input::IsMouseButtonPressed(1)) {
            glm::vec2 mouse_pos = { Input::GetMouseX(), Input::GetMouseY() };
            glm::vec2 delta = (mouse_pos - mInitialMousePosition) * 0.003f;
            mInitialMousePosition = mouse_pos;

            mYaw += delta.x;
            mPitch += delta.y;

            glm::quat orientation = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
            glm::vec3 forward = glm::rotate(orientation, glm::vec3(0.0f, 0.0f, -1.0f));
            glm::vec3 right = glm::rotate(orientation, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::vec3 up = glm::rotate(orientation, glm::vec3(0.0f, 1.0f, 0.0f));

            if (Input::IsKeyPressed('W')) mPosition += forward * mCameraSpeed * (float)ts;
            if (Input::IsKeyPressed('S')) mPosition -= forward * mCameraSpeed * (float)ts;
            if (Input::IsKeyPressed('A')) mPosition -= right * mCameraSpeed * (float)ts;
            if (Input::IsKeyPressed('D')) mPosition += right * mCameraSpeed * (float)ts;
            if (Input::IsKeyPressed('Q')) mPosition -= up * mCameraSpeed * (float)ts;
            if (Input::IsKeyPressed('E')) mPosition += up * mCameraSpeed * (float)ts;
        } else {
            mInitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
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
        glm::quat orientation = glm::quat(glm::vec3(-mPitch, -mYaw, 0.0f));
        mViewMatrix = glm::translate(glm::mat4(1.0f), mPosition) * glm::toMat4(orientation);
        mViewMatrix = glm::inverse(mViewMatrix);
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& event) {
        mFOV -= event.GetYOffset() * 2.0f;
        mFOV = std::clamp(mFOV, 1.0f, 120.0f);
        UpdateProjection();
        return false;
    }

} // namespace Loom