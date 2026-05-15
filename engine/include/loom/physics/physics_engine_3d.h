#pragma once

#include "loom/core/core.h"
#include <cstdint>

// Forward declarations keep Jolt's heavy headers out of engine-wide TUs.
namespace JPH {
    class TempAllocator;
    class JobSystem;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

namespace Loom {

    // Object layer constants exposed as plain integers so callers don't need
    // to pull Jolt headers just to assign a body's layer.
    namespace PhysicsLayers3D {
        constexpr uint16_t NON_MOVING = 0; // static geometry
        constexpr uint16_t MOVING     = 1; // dynamic / kinematic bodies
    }

    // Process-wide Jolt initialization. Owns the shared TempAllocator,
    // JobSystem, and layer-filter implementations that every per-scene
    // PhysicsSystem will reuse.
    class LOOM_API PhysicsEngine3D {
    public:
        static void Init();
        static void Shutdown();
        static bool IsInitialized();

        // Shared per-process resources — passed into JPH::PhysicsSystem::Update
        // and JPH::PhysicsSystem::Init by per-scene physics worlds (Slice B).
        static JPH::TempAllocator& GetTempAllocator();
        static JPH::JobSystem&     GetJobSystem();

        static const JPH::BroadPhaseLayerInterface&      GetBroadPhaseLayerInterface();
        static const JPH::ObjectVsBroadPhaseLayerFilter& GetObjectVsBroadPhaseLayerFilter();
        static const JPH::ObjectLayerPairFilter&         GetObjectLayerPairFilter();
    };

} // namespace Loom
