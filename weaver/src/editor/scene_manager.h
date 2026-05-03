#pragma once

#include "editor_context.h"
#include <string>

namespace Weaver {

    class SceneManager {
    public:
        explicit SceneManager(EditorContext& ctx);

        void NewScene();
        void OpenScene();
        void OpenScene(const std::string& filepath);
        void SaveScene();
        void SaveSceneAs();

        void OnScenePlay();
        void OnSceneStop();

        // Renders the "Save Changes?" modal — call each frame from OnImGuiRender
        void OnImGuiRender();

    private:
        void NewSceneImpl();
        void OpenSceneImpl(const std::string& filepath);

        EditorContext& mContext;

        bool mShowSavePrompt = false;

        enum class PendingAction { None, Open, New };
        PendingAction mPendingAction = PendingAction::None;
        std::string   mPendingPath;
    };

} // namespace Weaver