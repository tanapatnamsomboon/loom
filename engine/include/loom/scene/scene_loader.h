#pragma once

#include "loom/core/core.h"
#include <string>

namespace Loom {

    // Engine-side singleton for queuing scene transitions.
    // Scripts call Scene.Load / Scene.Reload, which queue here.
    // The application layer (EditorLayer / RuntimeLayer) polls each frame
    // and executes the transition via OnRuntimeSceneTransition.
    class LOOM_API SceneLoader {
    public:
        static SceneLoader& Get();

        void QueueLoad(const std::string& relative_path);
        void QueueReload();

        bool               HasPendingTransition() const;
        bool               IsReload()             const;
        const std::string& GetPendingPath()       const;

        void Consume();

    private:
        SceneLoader() = default;

        bool        mHasPending  = false;
        bool        mIsReload    = false;
        std::string mPendingPath;
    };

} // namespace Loom
