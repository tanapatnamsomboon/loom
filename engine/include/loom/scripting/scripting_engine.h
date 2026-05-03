#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include <memory>
#include <string>

namespace Loom {

    class Scene;
    class IScriptingBackend;

    // Singleton facade for the scripting subsystem. Owns the active backend and
    // routes all runtime lifecycle calls to it. Engine-internal code (e.g. Application)
    // is responsible for calling Init() with a concrete backend before runtime starts.
    class LOOM_API ScriptingEngine {
    public:
        static void Init();
        static void Shutdown();

        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeUpdate(Timestep ts, Scene* scene);
        static void OnRuntimeStop();

        // Forwarded by a file-watcher when a script asset changes on disk.
        static void OnFileChanged(const std::string& path);

        static bool HasBackend();

    private:
        static std::unique_ptr<IScriptingBackend> sBackend;
    };

} // namespace Loom
