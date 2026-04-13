#include "platform/linux/linux_window.h"
#include "loom/core/log.h"

namespace Loom {

    static bool sGLFWInitialized = false;

    Window* Window::Create(const WindowProps& props) {
        return new LinuxWindow(props);
    }

    LinuxWindow::LinuxWindow(const WindowProps& props) {
        Init(props);
    }

    LinuxWindow::~LinuxWindow() {
        Shutdown();
    }

    void LinuxWindow::OnUpdate() {
        glfwPollEvents();
        glfwSwapBuffers(mWindow);
    }

    void LinuxWindow::SetVSync(bool enabled) {
        if (enabled) glfwSwapInterval(1);
        else glfwSwapInterval(0);
        mData.VSync = enabled;
    }

    bool LinuxWindow::IsVSync() const {
        return mData.VSync;
    }

    void LinuxWindow::Init(const WindowProps &props) {
        mData.Title = props.Title;
        mData.Width = props.Width;
        mData.Height = props.Height;

        LOOM_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);

        if (!sGLFWInitialized) {
            int success = glfwInit();
            if (!success) {
                LOOM_CORE_ERROR("Could not initialize GLFW!");
            }
            sGLFWInitialized = true;
        }

        mWindow = glfwCreateWindow((int)props.Width, (int)props.Height, mData.Title.c_str(), nullptr, nullptr);
        glfwMakeContextCurrent(mWindow);
        glfwSetWindowUserPointer(mWindow, &mData);
        SetVSync(true);
    }

    void LinuxWindow::Shutdown() {
        glfwDestroyWindow(mWindow);
    }
} // namespace Loom