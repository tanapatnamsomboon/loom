#include "loom/core/input.h"
#include "loom/renderer/renderer_api.h"
#include "platform/glfw/glfw_input.h"

#if defined(LOOM_PLATFORM_WINDOWS)
#   include "platform/windows/windows_input.h"
#endif

namespace Loom {

    Input* Input::sInstance = nullptr;

    void Input::Create() {
        if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL) {
            sInstance = new GLFWInput();
            return;
        }

#if defined(LOOM_PLATFORM_WINDOWS)
        sInstance = new WindowsInput();
#endif
    }
} // namespace Loom