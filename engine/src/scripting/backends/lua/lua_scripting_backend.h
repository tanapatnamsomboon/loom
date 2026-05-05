#pragma once

#include "scripting/backends/scripting_backend.h"
#include "scripting/file_watcher.h"
#include <sol/sol.hpp>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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

        void OnSensorBegin(entt::entity a, entt::entity b) override;
        void OnSensorEnd(entt::entity a, entt::entity b) override;

        void OnFileChanged(const std::string& path) override;

        std::vector<ScriptField> GetScriptFields(const std::string& script_path) override;
        void ApplyFields(entt::entity entity,
                         const std::unordered_map<std::string, ScriptField>& fields) override;
        bool TryGetFieldValue(entt::entity entity,
                              const std::string& name,
                              ScriptField& out_field) override;

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
        // Cached field schemas keyed by absolute script path; invalidated by OnFileChanged.
        std::unordered_map<std::string, std::vector<ScriptField>> mFieldSchemaCache;
    };

} // namespace Loom
