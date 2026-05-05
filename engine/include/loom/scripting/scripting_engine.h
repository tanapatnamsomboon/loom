#pragma once

#include "loom/core/core.h"
#include "loom/core/timestep.h"
#include "loom/scene/entity.h"
#include <memory>
#include <string>

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

        // Forwarded by a file-watcher when a script asset changes on disk.
        static void OnFileChanged(const std::string& path);

        static bool HasBackend();

    private:
        static std::unique_ptr<IScriptingBackend> sBackend;
    };

} // namespace Loom
