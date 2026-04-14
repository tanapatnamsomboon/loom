#include "loom/core/window.h"

#ifdef LOOM_PLATFORM_WINDOWS
#   include "platform/windows/windows_window.h"
#elif defined(LOOM_PLATFORM_LINUX)
#   include "platform/linux/linux_window.h"
#endif

namespace Loom {

    Window* Window::Create(const WindowProps& props) {
#   if defined(LOOM_PLATFORM_WINDOWS)
        return new WindowsWindow(props);
#   elif defined(LOOM_PLATFORM_LINUX)
        return new LinuxWindow(props);
#   else
        return nullptr;
#   endif
    }

} // namespace Loom