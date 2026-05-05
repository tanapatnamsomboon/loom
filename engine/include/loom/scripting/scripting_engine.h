#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/scene/entity.h"
#include "loom/scripting/script_field.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loom {

    class Scene;
    class IScriptingBackend;

    // Singleton facade; forwards scripting lifecycle calls to the active backend.
    class LOOM_API ScriptingEngine {
    public:
        static void Init();
        static void Shutdown();

        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeUpdate(Timestep ts, Scene* scene);
        static void OnRuntimeStop();

        static void OnCollisionBegin(Entity a, Entity b);
        static void OnCollisionEnd(Entity a, Entity b);

        static void OnSensorBegin(Entity a, Entity b);
        static void OnSensorEnd(Entity a, Entity b);

        static void OnFileChanged(const std::string& path);

        // Returns the field schema from the script's top-level Properties table (cached).
        static std::vector<ScriptField> GetScriptFields(const std::string& script_path);

        // Injects editor-set field overrides into a live script environment.
        static void ApplyFields(Entity entity,
                                const std::unordered_map<std::string, ScriptField>& fields);

        // Reads the current runtime value of a named global.
        // out_field.Type must be set by the caller; returns false if no live script exists.
        static bool TryGetFieldValue(Entity entity,
                                     const std::string& name,
                                     ScriptField& out_field);

        static bool HasBackend();

    private:
        static std::unique_ptr<IScriptingBackend> sBackend;
    };

} // namespace Loom
