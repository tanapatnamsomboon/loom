#pragma once

#include "panels/content_browser_panel.h"
#include "panels/scene_hierarchy_panel.h"
#include <glm/glm.hpp>
#include <loom/core/layer.h>
#include <loom/events/key_event.h>
#include <loom/renderer/buffer.h>
#include <loom/renderer/framebuffer.h>
#include <loom/renderer/vertex_array.h>
#include <loom/renderer/shader.h>
#include <loom/scene/entity.h>
#include <loom/scene/scene.h>

namespace Weaver {

    enum class SceneState {
        Edit = 0,
        Play = 1
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
        void HandleViewportResize();
        void UpdateScene(Loom::Timestep ts);
        void HandleMousePicking();

        bool OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event);
        bool OnKeyPressed(Loom::KeyPressedEvent& event);
        void HandleShortcuts(Loom::KeyPressedEvent& event);
        void HandleGizmoTypeChange(Loom::KeyPressedEvent& event);

        void RenderMainMenuBar();
        void RenderModals();
        void RenderPanels();
        void RenderToolbar();
        void RenderViewport();
        void UpdateViewportBounds();
        void UpdateViewportSize();
        void RenderGizmos();

        void NewProject();
        void OpenProject();
        void OpenProject(const std::string& filepath);
        void SaveProjectAs();

        void RenderProjectWizard();

        void NewScene();
        void NewSceneImpl();
        void OpenScene();
        void OpenScene(const std::string& filepath);
        void OpenSceneImpl(const std::string& filepath);
        void SaveScene();
        void SaveSceneAs();

        void OnScenePlay();
        void OnSceneStop();

    private:
        bool mViewportFocused = false;
        bool mViewportHovered = false;

        std::shared_ptr<Loom::VertexArray>  mSkyboxVAO;
        std::shared_ptr<Loom::VertexBuffer> mSkyboxVBO;
        std::shared_ptr<Loom::Shader>       mSkyboxShader;

        struct GridSettings {
            float MinorScale     = 1.0f;
            float MajorScale     = 10.0f;
            float LineThickness  = 1.5f;
            float FadeStart      = 20.0f;
            float FadeEnd        = 80.0f;
            glm::vec4 MinorColor = { 0.3f, 0.3f, 0.3f, 0.3f };
            glm::vec4 MajorColor = { 0.5f, 0.5f, 0.5f, 0.6f };
        };
        GridSettings mGridSettings;

        std::shared_ptr<Loom::VertexArray>  mGridVAO;
        std::shared_ptr<Loom::VertexBuffer> mGridVBO;
        std::shared_ptr<Loom::Shader>       mGridShader;

        bool mShowSceneHierarchyPanel = true;
        bool mShowContentBrowserPanel = true;
        bool mShowAboutModal          = false;

        Loom::EditorCamera mEditorCamera;
        Loom::Entity       mHoveredEntity;

        SceneHierarchyPanel mSceneHierarchyPanel;
        ContentBrowserPanel mContentBrowserPanel;

        std::shared_ptr<Loom::Framebuffer> mFramebuffer;
        glm::vec2                          mViewportSize = { 0.0f, 0.0f };
        glm::vec2                          mViewportBounds[2];

        int mGizmoType = -1;

        std::string mCurrentScenePath;

        std::shared_ptr<Loom::Scene> mEditorScene;
        std::shared_ptr<Loom::Scene> mActiveScene;
        SceneState                   mSceneState = SceneState::Edit;

        bool mShowProjectWizard = false;
        char mNewProjectName[256] = "MyAwesomeGame";
        std::string mNewProjectPath = "";

        bool mSceneDirty = false;
        bool mShowSavePrompt = false;

        enum class SceneAction { None, Open, New };
        SceneAction mPendingSceneAction = SceneAction::None;
        std::string mPendingScenePath;
    };

} // namespace Weaver
