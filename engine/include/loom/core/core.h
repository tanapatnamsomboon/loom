#pragma once

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
