#pragma once

#include "editor_context.h"
#include "editor/scene_manager.h"
#include "panels/content_browser_panel.h"
#include <string>

namespace Weaver {

    class ProjectManager {
    public:
        ProjectManager(EditorContext& ctx, ContentBrowserPanel& contentBrowser, SceneManager& sceneManager);

        void ShowWizard() { mShowWizard = true; }

        void NewProject();
        void OpenProject();
        void OpenProject(const std::string& filepath);
        void SaveProjectAs();

        // Renders the "New Project Wizard" modal — call each frame from OnImGuiRender
        void OnImGuiRender();

    private:
        EditorContext&       mContext;
        ContentBrowserPanel& mContentBrowser;
        SceneManager&        mSceneManager;

        bool mShowWizard = false;
        char mProjectName[256] = "MyAwesomeGame";
        std::string mProjectPath;
    };

} // namespace Weaver