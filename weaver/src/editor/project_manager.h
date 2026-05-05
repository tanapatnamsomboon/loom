#pragma once

#include "editor_context.h"
#include "editor/scene_manager.h"
#include "editor/editor_prefs.h"
#include "panels/content_browser_panel.h"
#include <string>
#include <vector>

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

        const std::vector<std::string>& GetRecentProjects() const { return mPrefs.RecentProjects; }

        // Renders modals — call each frame from OnImGuiRender
        void OnImGuiRender();

    private:
        EditorContext&       mContext;
        ContentBrowserPanel& mContentBrowser;
        SceneManager&        mSceneManager;

        EditorPrefs mPrefs;
        std::string mDeferredOpenPath;

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

        void AddToRecent(const std::string& filepath);
    };

} // namespace Weaver
