#include "loom/core/window.h"
#include "loom/renderer/renderer_api.h"
#include "platform/glfw/glfw_window.h"
#ifdef LOOM_PLATFORM_WINDOWS
#   include "platform/windows/windows_window.h"
#endif

namespace Loom {

    Window* Window::Create(const WindowProps& props) {
        if (RendererAPI::GetAPI() == RendererAPI::API::OpenGL) {
            return new GLFWWindow(props);
        }

#   if defined(LOOM_PLATFORM_WINDOWS)
        return new WindowsWindow(props);
#   else
        LOOM_CORE_FATAL("Unknown platform or rendering API!");
        return nullptr;
#   endif
    }

} // namespace Loom