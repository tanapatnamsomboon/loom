#pragma once

#include "panels/scene_hierarchy_panel.h"
#include <loom/core/layer.h>
#include <loom/events/key_event.h>
#include <loom/scene/scene.h>
#include <loom/scene/entity.h>
#include <loom/renderer/framebuffer.h>
#include <glm/glm.hpp>

namespace Weaver {

    enum class SceneState {
        Edit = 0, Play = 1
    };

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

        void OnScenePlay();
        void OnSceneStop();

        bool OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event);
        bool OnKeyPressed(Loom::KeyPressedEvent& event);

    private:
        bool mViewportFocused = false;
        bool mViewportHovered = false;

        bool mShowHierarchyPanel = true;
        bool mShowAboutModal = false;

        Loom::EditorCamera mEditorCamera;
        Loom::Entity mHoveredEntity;

        SceneHierarchyPanel mHierarchyPanel;

        std::shared_ptr<Loom::Framebuffer> mFramebuffer;
        glm::vec2 mViewportSize = { 0.0f, 0.0f };
        glm::vec2 mViewportBounds[2];

        std::string mCurrentScenePath;

        int mGizmoType = -1;

        std::shared_ptr<Loom::Scene> mEditorScene;
        std::shared_ptr<Loom::Scene> mActiveScene;
        SceneState mSceneState = SceneState::Edit;
    };

} // namespace Weaver