#pragma once

#include "loom/renderer/graphics_context.h"
#include <Windows.h>

namespace Loom {

    class OpenGLContext : public GraphicsContext {
    public:
        OpenGLContext(HWND window_handle);

        void Init() override;
        void SwapBuffers() override;

    private:
        HWND mWindowHandle;
        HDC mDeviceContext;
        HGLRC mRenderContext;
    };

} // namespace Loom