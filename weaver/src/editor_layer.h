#pragma once

#include <loom/core/layer.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene_hierarchy_panel.h>
#include <loom/renderer/framebuffer.h>
#include <glm/glm.hpp>

namespace Weaver {

    class EditorLayer : public Loom::Layer {
    public:
        EditorLayer();
        ~EditorLayer() override = default;

        void OnAttach() override;
        void OnUpdate(Loom::Timestep ts) override;
        void OnImGuiRender() override;

    private:
        std::shared_ptr<Loom::Scene> mScene;
        Loom::SceneHierarchyPanel mHierarchyPanel;
        Loom::Entity mCameraEntity;

        std::shared_ptr<Loom::Framebuffer> mFramebuffer;
        glm::vec2 mViewportSize = { 0.0f, 0.0f };
    };

} // namespace Weaver