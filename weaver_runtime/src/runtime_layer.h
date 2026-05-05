#pragma once

#include <loom/core/layer.h>
#include <loom/core/timestep.h>
#include <loom/events/application_event.h>
#include <loom/scene/scene.h>
#include <filesystem>
#include <memory>

class RuntimeLayer : public Loom::Layer {
public:
    RuntimeLayer();

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(Loom::Timestep ts) override;
    void OnEvent(Loom::Event& event) override;

private:
    void LoadScene(const std::filesystem::path& scene_path);
    bool OnWindowResize(Loom::WindowResizeEvent& event);

    std::shared_ptr<Loom::Scene> mScene;
    std::filesystem::path        mCurrentScenePath;
    uint32_t                     mViewportWidth  = 0;
    uint32_t                     mViewportHeight = 0;
};
