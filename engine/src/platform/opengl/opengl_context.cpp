#include "platform/opengl/opengl_context.h"
#include "loom/core/log.h"
#include <glad/glad.h>

#include <GLFW/glfw3.h>

namespace Loom {

    OpenGLContext::OpenGLContext(GLFWwindow* window_handle)
        : mWindowHandle(window_handle) {
        if (!mWindowHandle)
            LOOM_CORE_FATAL("Window handle is null!");
    }

    void OpenGLContext::Init() {
        glfwMakeContextCurrent(mWindowHandle);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            LOOM_CORE_FATAL("Failed to initialize GLAD!");
        }

        LOOM_CORE_INFO("OpenGL Info (Linux):");
        LOOM_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        LOOM_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        LOOM_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void OpenGLContext::SwapBuffers() {
        glfwSwapBuffers(mWindowHandle);
    }

} // namespace Loom
