#include "platform/glfw/glfw_input.h"
#include "loom/core/application.h"
#include <GLFW/glfw3.h>

namespace Loom {

    bool GLFWInput::IsKeyPressedImpl(Key keycode) {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        auto state = glfwGetKey(pWindow, (int)keycode);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool GLFWInput::IsMouseButtonPressedImpl(Mouse button) {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        auto state = glfwGetMouseButton(pWindow, (int)button);
        return state == GLFW_PRESS;
    }

    std::pair<float, float> GLFWInput::GetMousePositionImpl() {
        auto pWindow = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
        double xpos, ypos;
        glfwGetCursorPos(pWindow, &xpos, &ypos);
        return { (float)xpos, (float)ypos };
    }

    float GLFWInput::GetMouseXImpl() {
        auto [x, y] = GetMousePositionImpl();
        return x;
    }

    float GLFWInput::GetMouseYImpl() {
        auto [x, y] = GetMousePositionImpl();
        return y;
    }

} // namespace Loom