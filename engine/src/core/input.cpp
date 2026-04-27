#include "loom/core/input.h"
#include "loom/renderer/renderer_api.h"
#include "platform/glfw/glfw_input.h"

#if defined(LOOM_PLATFORM_WINDOWS)
#   include "platform/windows/windows_input.h"
#endif

namespace Loom {

    std::unique_ptr<Input> Input::sInstance = nullptr;

    void Input::Create() {
        if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL) {
            sInstance = std::make_unique<GLFWInput>();
            return;
        }

#if defined(LOOM_PLATFORM_WINDOWS)
        sInstance = std::make_unique<WindowsInput>();
#endif
    }
} // namespace Loom