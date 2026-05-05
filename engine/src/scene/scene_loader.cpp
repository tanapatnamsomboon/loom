#include "loom/scene/scene_loader.h"

namespace Loom {

    SceneLoader& SceneLoader::Get() {
        static SceneLoader instance;
        return instance;
    }

    void SceneLoader::QueueLoad(const std::string& relative_path) {
        mHasPending  = true;
        mIsReload    = false;
        mPendingPath = relative_path;
    }

    void SceneLoader::QueueReload() {
        mHasPending  = true;
        mIsReload    = true;
        mPendingPath.clear();
    }

    bool SceneLoader::HasPendingTransition() const { return mHasPending; }
    bool SceneLoader::IsReload()             const { return mIsReload; }

    const std::string& SceneLoader::GetPendingPath() const { return mPendingPath; }

    void SceneLoader::Consume() {
        mHasPending  = false;
        mIsReload    = false;
        mPendingPath.clear();
    }

} // namespace Loom
