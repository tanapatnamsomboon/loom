#include "platform/linux/linux_input.h"
#include "loom/core/application.h"
#include <GLFW/glfw3.h>

namespace Loom {

    Input* Input::sInstance = new LinuxInput();

    bool LinuxInput::IsKeyPressedImpl(int keycode) {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        auto state = glfwGetKey(pWindow, keycode);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool LinuxInput::IsMouseButtonPressedImpl(int button) {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        auto state = glfwGetMouseButton(pWindow, button);
        return state == GLFW_PRESS;
    }

    std::pair<float, float> LinuxInput::GetMousePositionImpl() {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        double xpos, ypos;
        glfwGetCursorPos(pWindow, &xpos, &ypos);
        return { (float)xpos, (float)ypos };
    }

    float LinuxInput::GetMouseXImpl() {
        auto [x, y] = GetMousePositionImpl();
        return x;
    }

    float LinuxInput::GetMouseYImpl() {
        auto [x, y] = GetMousePositionImpl();
        return y;
    }

} // namespace Loom