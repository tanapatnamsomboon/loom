#pragma once

#include <functional>

#ifdef _WIN32
#   ifdef ENGINE_BUILD_DLL
#       define ENGINE_API __declspec(dllexport)
#   else
#       define ENGINE_API __declspec(dllimport)
#   endif
#elif defined(__linux__)
#   ifdef ENGINE_BUILD_DLL
#       define ENGINE_API __attribute__((visibility("default")))
#   else
#       define ENGINE_API
#   endif
#else
#   error "Loom Engine only supports Windows and Linux!"
#endif

#define BIT(x) (1 << x)

#define LOOM_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)