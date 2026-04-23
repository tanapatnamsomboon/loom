#pragma once

#include "panels/scene_hierarchy_panel.h"
#include <loom/core/layer.h>
#include <loom/events/key_event.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>
#include <loom/renderer/framebuffer.h>
#include <glm/glm.hpp>

namespace Weaver {

    class EditorLayer : public Loom::Layer {
    public:
        EditorLayer();
        ~EditorLayer() override = default;

        void OnAttach() override;

        void OnUpdate(Loom::Timestep ts) override;
        void OnEvent(Loom::Event& event) override;

        void OnImGuiRender() override;

    private:
        void NewScene();
        void OpenScene();
        void OpenScene(const std::string& filepath);
        void SaveScene();
        void SaveSceneAs();

        bool OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event);
        bool OnKeyPressed(Loom::KeyPressedEvent& event);

    private:
        bool mViewportFocused = false;
        bool mViewportHovered = false;

        bool mShowHierarchyPanel = true;
        bool mShowAboutModal = false;

        Loom::EditorCamera mEditorCamera;
        Loom::Entity mHoveredEntity;

        std::shared_ptr<Loom::Scene> mScene;
        SceneHierarchyPanel mHierarchyPanel;

        std::shared_ptr<Loom::Framebuffer> mFramebuffer;
        glm::vec2 mViewportSize = { 0.0f, 0.0f };
        glm::vec2 mViewportBounds[2];

        std::string mCurrentScenePath;

        int mGizmoType = -1;
    };

} // namespace Weaver