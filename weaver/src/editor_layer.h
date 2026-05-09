#pragma once

#include "editor_context.h"
#include "editor/project_manager.h"
#include "editor/scene_manager.h"
#include "panels/content_browser_panel.h"
#include "panels/scene_hierarchy_panel.h"
#include "panels/toolbar_panel.h"
#include "panels/viewport_panel.h"
#include <loom/core/layer.h>
#include <loom/events/application_event.h>
#include <loom/events/key_event.h>

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
        bool OnWindowClose(Loom::WindowCloseEvent& event);
        bool OnMouseButtonPressed(Loom::MouseButtonPressedEvent& event);
        bool OnKeyPressed(Loom::KeyPressedEvent& event);
        void HandleShortcuts(Loom::KeyPressedEvent& event);

        void RenderMainMenuBar();
        void RenderAboutModal();

    private:
        EditorContext mContext;

        // Panels (own their UI state)
        SceneHierarchyPanel mSceneHierarchyPanel;
        ContentBrowserPanel mContentBrowserPanel;
        ViewportPanel       mViewportPanel;
        ToolbarPanel        mToolbarPanel;

        // Managers (own business logic + modals)
        SceneManager   mSceneManager;
        ProjectManager mProjectManager;

        bool mShowSceneHierarchyPanel = true;
        bool mShowContentBrowserPanel = true;
        bool mShowAboutModal          = false;
    };

} // namespace Weaver
