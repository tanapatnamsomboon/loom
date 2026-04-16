#include "loom/core/application.h"
#include "loom/core/input.h"
#include "loom/core/log.h"

namespace Loom {

    Application* Application::sInstance = nullptr;

    Application::Application() {
        sInstance = this;

        mWindow = std::unique_ptr<Window>(Window::Create());
        mWindow->SetEventCallback(LOOM_BIND_EVENT_FN(Application::OnEvent));

        Input::Create();

        mImGuiLayer = new ImGuiLayer();
        PushOverlay(mImGuiLayer);
    }

    Application::~Application() {}

    void Application::Run() {
        while (mRunning) {
            for (Layer* layer : mLayerStack)
                layer->OnUpdate();

            mImGuiLayer->Begin();
            for (Layer* layer : mLayerStack)
                layer->OnImGuiRender();
            mImGuiLayer->End();

            mWindow->OnUpdate();
        }
    }

    void Application::OnEvent(Event& event) {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(LOOM_BIND_EVENT_FN(Application::OnWindowClose));

        for (auto it = mLayerStack.end(); it != mLayerStack.begin(); ) {
            (*--it)->OnEvent(event);
            if (event.mHandled)
                break;
        }
    }

    void Application::PushLayer(Layer* layer) {
        mLayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(Layer* layer) {
        mLayerStack.PushOverlay(layer);
    }

    bool Application::OnWindowClose(WindowCloseEvent& event) {
        mRunning = false;
        return true;
    }
} // namespace Loom