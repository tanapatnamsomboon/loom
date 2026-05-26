#include "loom/physics/physics_engine_3d.h"
#include "loom/core/log.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <cstdarg>
#include <thread>

JPH_SUPPRESS_WARNINGS

namespace Loom {

    // ---- Layer plumbing -----------------------------------------------------
    // Two-layer split is the standard Jolt starter pattern: static vs. dynamic.
    // Adding gameplay layers (e.g. PLAYER, ENEMY) means widening these tables
    // and updating the ShouldCollide matrices below.

    namespace BPLayer3D {
        static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        static constexpr JPH::BroadPhaseLayer MOVING(1);
        static constexpr JPH::uint            NUM_LAYERS = 2;
    }

    static constexpr JPH::uint kNumObjectLayers = 2;

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            mObjectToBroadPhase[PhysicsLayers3D::NON_MOVING] = BPLayer3D::NON_MOVING;
            mObjectToBroadPhase[PhysicsLayers3D::MOVING]     = BPLayer3D::MOVING;
        }
        JPH::uint GetNumBroadPhaseLayers() const override { return BPLayer3D::NUM_LAYERS; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
            return mObjectToBroadPhase[layer];
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const override { return "Loom"; }
#endif

    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[kNumObjectLayers];
    };

    class ObjectVsBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer obj, JPH::BroadPhaseLayer bp) const override {
            switch (obj) {
                case PhysicsLayers3D::NON_MOVING: return bp == BPLayer3D::MOVING;
                case PhysicsLayers3D::MOVING:     return true;
                default:                          return false;
            }
        }
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
            switch (a) {
                case PhysicsLayers3D::NON_MOVING: return b == PhysicsLayers3D::MOVING;
                case PhysicsLayers3D::MOVING:     return true;
                default:                          return false;
            }
        }
    };

    // ---- Trace / assert hookups (route Jolt's logging through spdlog) -------

    static void JoltTraceImpl(const char* fmt, ...) {
        char    buffer[1024];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        LOOM_CORE_TRACE("Jolt: {}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool JoltAssertFailedImpl(const char* expression, const char* message,
                                     const char* file, JPH::uint line) {
        LOOM_CORE_ERROR("Jolt assert at {}:{}: {} ({})", file, (uint32_t)line, expression, message ? message : "");
        return true; // trigger debug breakpoint
    }
#endif

    // ---- Singleton state ----------------------------------------------------

    struct PhysicsEngine3DState {
        JPH::TempAllocator*         TempAllocator         = nullptr;
        JPH::JobSystem*             JobSystem             = nullptr;
        BPLayerInterfaceImpl*       BPLayerInterface      = nullptr;
        ObjectVsBPLayerFilterImpl*  ObjectVsBPFilter      = nullptr;
        ObjectLayerPairFilterImpl*  ObjectLayerPairFilter = nullptr;
        bool                        Initialized           = false;
    };
    static PhysicsEngine3DState sState;

    // ---- Lifecycle ----------------------------------------------------------

    void PhysicsEngine3D::Init() {
        if (sState.Initialized) {
            LOOM_CORE_WARN("PhysicsEngine3D::Init called twice - ignored");
            return;
        }

        JPH::RegisterDefaultAllocator();

        JPH::Trace = JoltTraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl;)

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        // 10MB scratch — Jolt's recommended starting size for a typical scene.
        sState.TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

        // One worker per logical core minus 1 (leave the main thread alone).
        const int worker_count = std::max(1, (int)std::thread::hardware_concurrency() - 1);
        sState.JobSystem       = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, worker_count);

        sState.BPLayerInterface      = new BPLayerInterfaceImpl();
        sState.ObjectVsBPFilter      = new ObjectVsBPLayerFilterImpl();
        sState.ObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

        sState.Initialized = true;
        LOOM_CORE_INFO("PhysicsEngine3D: Jolt initialized ({} worker threads)", worker_count);
    }

    void PhysicsEngine3D::Shutdown() {
        if (!sState.Initialized) return;

        delete sState.ObjectLayerPairFilter;  sState.ObjectLayerPairFilter = nullptr;
        delete sState.ObjectVsBPFilter;     sState.ObjectVsBPFilter    = nullptr;
        delete sState.BPLayerInterface;     sState.BPLayerInterface    = nullptr;
        delete sState.JobSystem;            sState.JobSystem           = nullptr;
        delete sState.TempAllocator;        sState.TempAllocator       = nullptr;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;

        sState.Initialized = false;
        LOOM_CORE_INFO("PhysicsEngine3D: Jolt shutdown");
    }

    bool PhysicsEngine3D::IsInitialized() {
        return sState.Initialized;
    }

    JPH::TempAllocator& PhysicsEngine3D::GetTempAllocator()                                 { return *sState.TempAllocator; }
    JPH::JobSystem&     PhysicsEngine3D::GetJobSystem()                                     { return *sState.JobSystem; }
    const JPH::BroadPhaseLayerInterface&      PhysicsEngine3D::GetBroadPhaseLayerInterface()      { return *sState.BPLayerInterface; }
    const JPH::ObjectVsBroadPhaseLayerFilter& PhysicsEngine3D::GetObjectVsBroadPhaseLayerFilter() { return *sState.ObjectVsBPFilter; }
    const JPH::ObjectLayerPairFilter&         PhysicsEngine3D::GetObjectLayerPairFilter()         { return *sState.ObjectLayerPairFilter; }

} // namespace Loom
