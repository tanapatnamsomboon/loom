#pragma once

#include "loom/core/core.h"
#include <string>

namespace Loom {

    struct AudioSourceComponent;

    class LOOM_API AudioEngine {
    public:
        static void Init();
        static void Shutdown();

        // Loads and starts playback for a source component. full_path must be an absolute filesystem path.
        static void PlaySource(AudioSourceComponent& src, const std::string& full_path);

        // Stops and releases the runtime sound owned by a source component.
        static void StopSource(AudioSourceComponent& src);
    };

} // namespace Loom
