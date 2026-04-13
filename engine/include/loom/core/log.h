#pragma once

#include "loom/core/core.h"
#include <spdlog/spdlog.h>
#include <memory>

namespace Loom {

    class ENGINE_API Log {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return sCoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return sClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> sCoreLogger;
        static std::shared_ptr<spdlog::logger> sClientLogger;
    };

} // namespace Loom

#define LOOM_CORE_TRACE(...)    ::Loom::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define LOOM_CORE_INFO(...)     ::Loom::Log::GetCoreLogger()->info(__VA_ARGS__)
#define LOOM_CORE_WARN(...)     ::Loom::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define LOOM_CORE_ERROR(...)    ::Loom::Log::GetCoreLogger()->error(__VA_ARGS__)
#define LOOM_CORE_FATAL(...)    ::Loom::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define LOOM_TRACE(...)    ::Loom::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LOOM_INFO(...)     ::Loom::Log::GetClientLogger()->info(__VA_ARGS__)
#define LOOM_WARN(...)     ::Loom::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LOOM_ERROR(...)    ::Loom::Log::GetClientLogger()->error(__VA_ARGS__)
#define LOOM_FATAL(...)    ::Loom::Log::GetClientLogger()->critical(__VA_ARGS__)