#pragma once

#include "scripting/backends/scripting_backend.h"
#include "scripting/file_watcher.h"
#include <sol/sol.hpp>
#include <entt/entt.hpp>
#include <unordered_map>
#include <memory>

namespace Loom {

    class LuaScriptingBackend : public IScriptingBackend {
    public:
        LuaScriptingBackend();
        ~LuaScriptingBackend() override = default;

        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeUpdate(Timestep ts, Scene* scene) override;
        void OnRuntimeStop() override;

        void OnCollisionBegin(entt::entity a, entt::entity b) override;
        void OnCollisionEnd(entt::entity a, entt::entity b) override;

        void OnFileChanged(const std::string& path) override;

    private:
        void BindLuaAPI();
        void LoadEntityScript(entt::entity entity_id, Scene* scene);
        void UnloadEntityScript(entt::entity entity_id);
        void DispatchCollisionEvent(entt::entity self, entt::entity other, const char* fn_name);

    private:
        sol::state mLua;
        std::unordered_map<entt::entity, sol::environment> mScriptInstances;
        Scene* mActiveScene = nullptr;
        std::unique_ptr<FileWatcher> mFileWatcher;
    };

} // namespace Loom
