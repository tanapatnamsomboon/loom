#pragma once

#include "loom/core/core.h"
#include "loom/renderer/graphics_context.h"

#ifdef LOOM_PLATFORM_WINDOWS
#   include <Windows.h>
#endif

struct GLFWwindow;

namespace Loom {

    class OpenGLContext : public GraphicsContext {
    public:
#   if defined(LOOM_PLATFORM_WINDOWS)
        OpenGLContext(HWND window_handle);
#   elif defined(LOOM_PLATFORM_LINUX)
        OpenGLContext(GLFWwindow* window_handle)
#   endif

        void Init() override;
        void SwapBuffers() override;

    private:
#   if defined(LOOM_PLATFORM_WINDOWS)
        HWND mWindowHandle;
        HDC mDeviceContext;
        HGLRC mRenderContext;
#   elif defined(LOOM_PLATFORM_LINUX)
        GLFWwindow* mWindowHandle;
#   endif
    };

} // namespace Loom