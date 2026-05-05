#pragma once

#include <loom/core/timestep.h>
#include <loom/scripting/script_field.h>
#include <entt/entt.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loom {

    class Scene;

    class IScriptingBackend {
    public:
        virtual ~IScriptingBackend() = default;

        virtual void OnRuntimeStart(Scene* scene)               = 0;
        virtual void OnRuntimeUpdate(Timestep ts, Scene* scene) = 0;
        virtual void OnRuntimeStop()                            = 0;

        virtual void OnCollisionBegin(entt::entity a, entt::entity b) = 0;
        virtual void OnCollisionEnd(entt::entity a, entt::entity b)   = 0;

        virtual void OnSensorBegin(entt::entity a, entt::entity b) = 0;
        virtual void OnSensorEnd(entt::entity a, entt::entity b)   = 0;

        virtual void OnFileChanged(const std::string& path) = 0;

        // Returns the field schema declared in the script's top-level Properties table.
        virtual std::vector<ScriptField> GetScriptFields(const std::string& script_path) = 0;

        // Injects editor-set field overrides into a live script environment.
        virtual void ApplyFields(entt::entity entity,
                                 const std::unordered_map<std::string, ScriptField>& fields) = 0;

        // Reads the current runtime value of a named global from a live script environment.
        // out_field.Type must be set by the caller; returns false if the entity has no live script.
        virtual bool TryGetFieldValue(entt::entity entity,
                                      const std::string& name,
                                      ScriptField& out_field) = 0;
    };

} // namespace Loom
