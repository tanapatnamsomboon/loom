#include "loom/core/log.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

namespace Loom {

    std::shared_ptr<spdlog::logger> Log::sCoreLogger;
    std::shared_ptr<spdlog::logger> Log::sClientLogger;

    void Log::Init() {
        // File is truncated each run on purpose — one session per log makes
        // "what just happened" easier to read than a rolling history.
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_pattern("%^[%T] %n: %v%$");

        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink;
        try {
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/loom.log", /*truncate*/ true);
            file_sink->set_pattern("[%Y-%m-%d %T.%e] [%-5l] %n: %v");
        } catch (const spdlog::spdlog_ex&) {
            // CWD not writable — keep engine init alive on console-only.
            file_sink.reset();
        }

        std::vector<spdlog::sink_ptr> sinks { stdout_sink };
        if (file_sink) sinks.push_back(file_sink);

        sCoreLogger   = std::make_shared<spdlog::logger>("LOOM", sinks.begin(), sinks.end());
        sCoreLogger->set_level(spdlog::level::trace);
        sCoreLogger->flush_on(spdlog::level::warn);
        spdlog::register_logger(sCoreLogger);

        sClientLogger = std::make_shared<spdlog::logger>("APP",  sinks.begin(), sinks.end());
        sClientLogger->set_level(spdlog::level::trace);
        sClientLogger->flush_on(spdlog::level::warn);
        spdlog::register_logger(sClientLogger);
    }

} // namespace Loom