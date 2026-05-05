#include "runtime_layer.h"
#include <loom/core/application.h>
#include <loom/core/log.h>
#include <loom/project/project.h>
#include <loom/renderer/render_command.h>
#include <loom/scene/scene_loader.h>
#include <loom/scene/scene_serializer.h>

RuntimeLayer::RuntimeLayer()
    : Layer("RuntimeLayer") {}

void RuntimeLayer::OnAttach() {
    auto project = Loom::Project::GetActive();
    if (!project) {
        LOOM_ERROR("RuntimeLayer: no active project");
        Loom::Application::Get().Close();
        return;
    }

    const auto& config = project->GetConfig();
    if (config.StartScene.empty()) {
        LOOM_ERROR("RuntimeLayer: no StartScene configured in project '{}'", config.Name);
        Loom::Application::Get().Close();
        return;
    }

    auto& window   = Loom::Application::Get().GetWindow();
    mViewportWidth  = window.GetWidth();
    mViewportHeight = window.GetHeight();

    LoadScene(Loom::Project::GetAssetFileSystemPath(config.StartScene));
}

void RuntimeLayer::OnDetach() {
    if (mScene)
        mScene->OnRuntimeStop();
}

void RuntimeLayer::OnUpdate(Loom::Timestep ts) {
    if (!mScene)
        return;

    Loom::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    Loom::RenderCommand::Clear();

    mScene->OnUpdateRuntime(ts);

    // Poll scene transitions queued by Lua scripts (Scene.Load / Scene.Reload)
    auto& loader = Loom::SceneLoader::Get();
    if (loader.HasPendingTransition()) {
        mScene->OnRuntimeStop();

        std::filesystem::path next_path = loader.IsReload()
            ? mCurrentScenePath
            : Loom::Project::GetAssetFileSystemPath(loader.GetPendingPath());
        loader.Consume();

        LoadScene(next_path);
    }
}

void RuntimeLayer::OnEvent(Loom::Event& event) {
    Loom::EventDispatcher dispatcher(event);
    dispatcher.Dispatch<Loom::WindowResizeEvent>(
        LOOM_BIND_EVENT_FN(RuntimeLayer::OnWindowResize));
}

void RuntimeLayer::LoadScene(const std::filesystem::path& scene_path) {
    mScene = std::make_shared<Loom::Scene>();
    Loom::SceneSerializer serializer(mScene);
    if (!serializer.Deserialize(scene_path.string())) {
        LOOM_ERROR("RuntimeLayer: failed to load scene '{}'", scene_path.string());
        mScene.reset();
        return;
    }

    mCurrentScenePath = scene_path;
    mScene->OnViewportResize(mViewportWidth, mViewportHeight);
    mScene->OnRuntimeStart();
}

bool RuntimeLayer::OnWindowResize(Loom::WindowResizeEvent& event) {
    mViewportWidth  = event.GetWidth();
    mViewportHeight = event.GetHeight();
    if (mScene)
        mScene->OnViewportResize(mViewportWidth, mViewportHeight);
    return false;
}
