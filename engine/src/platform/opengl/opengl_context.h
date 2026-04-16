#pragma once

#include "loom/renderer/graphics_context.h"


struct GLFWwindow;

namespace Loom {

    class OpenGLContext : public GraphicsContext {
    public:
        OpenGLContext(GLFWwindow* window_handle);

        void Init() override;
        void SwapBuffers() override;

    private:
        GLFWwindow* mWindowHandle;
    };

} // namespace Loom