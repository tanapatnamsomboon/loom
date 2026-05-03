#pragma once

#include <loom/core/timestep.h>
#include <string>

namespace Loom {

    class Scene;

    class IScriptingBackend {
    public:
        virtual ~IScriptingBackend() = default;

        virtual void OnRuntimeStart(Scene* scene)         = 0;
        virtual void OnRuntimeUpdate(Timestep ts, Scene* scene) = 0;
        virtual void OnRuntimeStop()                      = 0;

        // Invoked by the ScriptingEngine file watcher when a script file changes on disk.
        virtual void OnFileChanged(const std::string& path) = 0;
    };

} // namespace Loom
