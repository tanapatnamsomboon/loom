#pragma once

#ifdef _WIN32
#   define LOOM_PLATFORM_WINDOWS
#   ifdef LOOM_BUILD_DLL
#       define LOOM_API __declspec(dllexport)
#   else
#       define LOOM_API __declspec(dllimport)
#   endif
#elif defined(__linux__)
#   define LOOM_PLATFORM_LINUX
#   ifdef LOOM_BUILD_DLL
#       define LOOM_API __attribute__((visibility("default")))
#   else
#       define LOOM_API
#   endif
#else
#   error "Loom Engine only supports Windows and Linux!"
#endif

#include <functional>

#define BIT(x) (1 << x)
#define LOOM_BIND_EVENT_FN(fn) [this](auto&& event) { return fn(std::forward<decltype(event)>(event)); }