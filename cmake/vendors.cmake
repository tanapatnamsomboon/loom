# =============================================================================
# vendors.cmake
# All third-party dependency configuration in one place.
# =============================================================================

# -----------------------------------------------------------------------------
# spdlog
# -----------------------------------------------------------------------------
set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS   OFF CACHE INTERNAL "")
add_subdirectory(vendor/spdlog)

# -----------------------------------------------------------------------------
# GLFW
# -----------------------------------------------------------------------------
set(GLFW_BUILD_DOCS     OFF CACHE INTERNAL "")
set(GLFW_BUILD_TESTS    OFF CACHE INTERNAL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GLFW_INSTALL        OFF CACHE INTERNAL "")
add_subdirectory(vendor/glfw)

# -----------------------------------------------------------------------------
# glad
# -----------------------------------------------------------------------------
add_subdirectory(vendor/glad)

# -----------------------------------------------------------------------------
# glm
# -----------------------------------------------------------------------------
add_subdirectory(vendor/glm)

# -----------------------------------------------------------------------------
# stb
# -----------------------------------------------------------------------------
add_library(stb STATIC vendor/stb/stb_image.cpp vendor/stb/stb_truetype.cpp)
target_include_directories(stb PUBLIC vendor/stb)

# -----------------------------------------------------------------------------
# EnTT
# -----------------------------------------------------------------------------
add_subdirectory(vendor/entt)

# -----------------------------------------------------------------------------
# yaml-cpp
# -----------------------------------------------------------------------------
set(YAML_CPP_BUILD_TESTS   OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_TOOLS   OFF CACHE INTERNAL "")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE INTERNAL "")
add_subdirectory(vendor/yaml-cpp)

# -----------------------------------------------------------------------------
# Dear ImGui
# -----------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PRIVATE glfw glad)

# -----------------------------------------------------------------------------
# ImGuizmo
# -----------------------------------------------------------------------------
set(IMGUIZMO_DIR ${CMAKE_SOURCE_DIR}/vendor/imguizmo)

add_library(imguizmo STATIC
    ${IMGUIZMO_DIR}/ImGuizmo.cpp
)
target_include_directories(imguizmo PUBLIC ${IMGUIZMO_DIR})
target_link_libraries(imguizmo PRIVATE imgui)

# -----------------------------------------------------------------------------
# ImGuiFileDialog
# -----------------------------------------------------------------------------
set(IMGUIFILEDIALOG_DIR ${CMAKE_SOURCE_DIR}/vendor/imguifiledialog)

add_library(imguifiledialog STATIC
    ${IMGUIFILEDIALOG_DIR}/ImGuiFileDialog.cpp
)
target_include_directories(imguifiledialog PUBLIC ${IMGUIFILEDIALOG_DIR})
target_link_libraries(imguifiledialog PUBLIC imgui)

# -----------------------------------------------------------------------------
# Box2D
# -----------------------------------------------------------------------------
set(BOX2D_BUILD_TESTBED OFF CACHE INTERNAL "")
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE INTERNAL "")
set(BOX2D_BUILD_DOCS OFF CACHE INTERNAL "")
add_subdirectory(vendor/box2d)

# -----------------------------------------------------------------------------
# Lua 5.4
# (The official Lua repo ships no CMakeLists.txt; compile the sources directly.
#  Exclude: lua.c / luac.c — standalone binaries with their own main();
#           onelua.c      — single-file amalgamation (would duplicate every TU);
#           ltests.c      — internal test infrastructure.)
# -----------------------------------------------------------------------------
file(GLOB LUA_SOURCES "vendor/lua/*.c")
list(FILTER LUA_SOURCES EXCLUDE REGEX "(lua|luac|onelua|ltests)\\.c$")
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC vendor/lua)
if(MSVC)
    target_compile_definitions(lua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# -----------------------------------------------------------------------------
# sol2 v3.3.0 (header-only)
# sol2 headers include <lua.h> directly, so lua must be an INTERFACE dependency
# so that lua's public include directory (vendor/lua) is on the include path
# for every target that pulls in sol2.
# -----------------------------------------------------------------------------
add_library(sol2 INTERFACE)
target_include_directories(sol2 INTERFACE vendor/sol2/include)
target_link_libraries(sol2 INTERFACE lua)

# -----------------------------------------------------------------------------
# miniaudio (single-header C audio library)
# MINIAUDIO_IMPLEMENTATION must be defined in exactly one .cpp (audio_engine.cpp)
# -----------------------------------------------------------------------------
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE vendor/miniaudio)
if(WIN32)
    target_link_libraries(miniaudio INTERFACE ole32 advapi32)
endif()
