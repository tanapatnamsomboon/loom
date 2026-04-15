#include "platform/opengl/opengl_context.h"
#include "loom/core/log.h"
#include <glad/glad.h>

namespace Loom {

    static void* GetAnyGLFuncAddress(const char* name) {
        void* p = (void*)wglGetProcAddress(name);
        if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3) {
            HMODULE module = LoadLibraryW(L"opengl32.dll");
            p = (void*)GetProcAddress(module, name);
        }
        return p;
    }

    OpenGLContext::OpenGLContext(HWND window_handle)
        : mWindowHandle(window_handle), mDeviceContext(nullptr), mRenderContext(nullptr) {
        if (!mWindowHandle) {
            LOOM_CORE_FATAL("Window handle is null!");
        }
    }

    void OpenGLContext::Init() {
        mDeviceContext = GetDC(mWindowHandle);

        PIXELFORMATDESCRIPTOR pfd = { 0 };
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;

        int format = ChoosePixelFormat(mDeviceContext, &pfd);
        if (!SetPixelFormat(mDeviceContext, format, &pfd)) {
            LOOM_CORE_FATAL("Fatal to set pixel format!");
        }

        mRenderContext = wglCreateContext(mDeviceContext);
        if (!wglMakeCurrent(mDeviceContext, mRenderContext)) {
            LOOM_CORE_FATAL("Failed to make OpenGL context current!");
        }

        if (!gladLoadGLLoader((GLADloadproc)GetAnyGLFuncAddress)) {
            LOOM_CORE_FATAL("Failed to initialize GLAD!");
        }

        LOOM_CORE_INFO("OpenGL Info:");
        LOOM_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        LOOM_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        LOOM_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void OpenGLContext::SwapBuffers() {
        ::SwapBuffers(mDeviceContext);
    }

} // namespace Loom