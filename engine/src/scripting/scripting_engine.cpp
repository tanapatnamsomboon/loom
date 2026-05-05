#include "loom/scripting/scripting_engine.h"
#include "scripting/backends/scripting_backend.h"
#include "scripting/backends/lua/lua_scripting_backend.h"
#include "loom/core/log.h"

namespace Loom {

    std::unique_ptr<IScriptingBackend> ScriptingEngine::sBackend = nullptr;

    void ScriptingEngine::Init() {
        sBackend = std::make_unique<LuaScriptingBackend>();
        LOOM_CORE_INFO("ScriptingEngine initialized (Lua backend).");
    }

    void ScriptingEngine::Shutdown() {
        sBackend.reset();
        LOOM_CORE_INFO("ScriptingEngine shut down.");
    }

    void ScriptingEngine::OnRuntimeStart(Scene* scene) {
        if (sBackend)
            sBackend->OnRuntimeStart(scene);
    }

    void ScriptingEngine::OnRuntimeUpdate(Timestep ts, Scene* scene) {
        if (sBackend)
            sBackend->OnRuntimeUpdate(ts, scene);
    }

    void ScriptingEngine::OnRuntimeStop() {
        if (sBackend)
            sBackend->OnRuntimeStop();
    }

    void ScriptingEngine::OnCollisionBegin(Entity a, Entity b) {
        if (sBackend)
            sBackend->OnCollisionBegin((entt::entity)a, (entt::entity)b);
    }

    void ScriptingEngine::OnCollisionEnd(Entity a, Entity b) {
        if (sBackend)
            sBackend->OnCollisionEnd((entt::entity)a, (entt::entity)b);
    }

    void ScriptingEngine::OnFileChanged(const std::string& path) {
        if (sBackend)
            sBackend->OnFileChanged(path);
    }

    bool ScriptingEngine::HasBackend() {
        return sBackend != nullptr;
    }

} // namespace Loom
