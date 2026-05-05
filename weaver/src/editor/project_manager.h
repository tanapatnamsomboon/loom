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
        void OpenSettings();

        // Renders the "New Project Wizard" modal — call each frame from OnImGuiRender
        void OnImGuiRender();

    private:
        EditorContext&       mContext;
        ContentBrowserPanel& mContentBrowser;
        SceneManager&        mSceneManager;

        bool mShowWizard        = false;
        bool mShowErrorModal    = false;
        bool mShowSettingsModal = false;
        std::string mErrorMessage;
        char mProjectName[256] = "MyAwesomeGame";
        std::string mProjectPath;

        // Temporary buffers used by the Project Settings modal
        char mSettingsName[256]        = {};
        char mSettingsStartScene[512]  = {};
        char mSettingsWindowTitle[256] = {};
        int  mSettingsWindowWidth      = 1280;
        int  mSettingsWindowHeight     = 720;
    };

} // namespace Weaver