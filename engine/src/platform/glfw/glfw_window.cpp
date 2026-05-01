#include "platform/glfw/glfw_window.h"
#include "platform/opengl/opengl_context.h"
#include "loom/core/log.h"
#include "loom/events/application_event.h"
#include "loom/events/mouse_event.h"
#include "loom/events/key_event.h"
#include <stb_image.h>

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

    void GLFWWindow::SetTitle(const std::string& title) {
        mData.Title = title;
        glfwSetWindowTitle(mWindow, mData.Title.c_str());
    }

    void GLFWWindow::SetIcon(const std::string& filepath) {
        GLFWimage images[1];
        int channels;

        images[0].pixels = stbi_load(filepath.c_str(), &images[0].width, &images[0].height, &channels, 4);

        if (images[0].pixels) {
            glfwSetWindowIcon(mWindow, 1, images);
            stbi_image_free(images[0].pixels);
        } else {
            LOOM_CORE_WARN("Failed to load window icon from: {0}", filepath);
        }
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

        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

        mWindow = glfwCreateWindow((int)props.Width, (int)props.Height, mData.Title.c_str(), nullptr, nullptr);
        mContext = std::make_unique<OpenGLContext>(mWindow);
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
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
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