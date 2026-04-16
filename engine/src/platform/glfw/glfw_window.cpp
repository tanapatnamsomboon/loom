#include "platform/glfw/glfw_window.h"
#include "platform/opengl/opengl_context.h"
#include "loom/core/log.h"
#include "loom/events/application_event.h"
#include "loom/events/mouse_event.h"
#include "loom/events/key_event.h"

namespace Loom {

    static bool sGLFWInitialized = false;

    GLFWWindow::GLFWWindow(const WindowProps& props) {
        Init(props);
    }

    GLFWWindow::~GLFWWindow() {
        Shutdown();
    }

    void GLFWWindow::OnUpdate() {
        glfwPollEvents();
        mContext->SwapBuffers();
    }

    void GLFWWindow::SetVSync(bool enabled) {
        if (enabled) glfwSwapInterval(1);
        else glfwSwapInterval(0);
        mData.VSync = enabled;
    }

    bool GLFWWindow::IsVSync() const {
        return mData.VSync;
    }

    void GLFWWindow::Init(const WindowProps &props) {
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
        mContext = new OpenGLContext(mWindow);
        mContext->Init();
        glfwSetWindowUserPointer(mWindow, &mData);
        SetVSync(true);

        glfwSetWindowSizeCallback(mWindow, [](GLFWwindow* pWindow, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);
            data.Width = width;
            data.Height = height;

            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });

        glfwSetWindowCloseCallback(mWindow, [](GLFWwindow* pWindow) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);

            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetKeyCallback(mWindow, [](GLFWwindow* pWindow, int key, int scancode, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);

            switch (action) {
                case GLFW_PRESS: {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT: {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
                default: break;
            }
        });

        glfwSetMouseButtonCallback(mWindow, [](GLFWwindow* pWindow, int button, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);

            switch (action) {
                case GLFW_PRESS: {
                    break;
                }
                case GLFW_RELEASE: {
                    break;
                }
                default: break;
            }
        });

        glfwSetScrollCallback(mWindow, [](GLFWwindow* pWindow, double xOffset, double yOffset) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);

            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(mWindow, [](GLFWwindow* pWindow, double xPos, double yPos) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(pWindow);

            MouseMovedEvent event((float)xPos, (float)yPos);
            data.EventCallback(event);
        });
    }

    void GLFWWindow::Shutdown() {
        glfwDestroyWindow(mWindow);
    }
} // namespace Loom