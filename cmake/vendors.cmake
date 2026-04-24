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
add_library(stb STATIC vendor/stb/stb_image.cpp)
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
    ${IMGUIZMO_DIR}/imguizmo.cpp
)
target_include_directories(imguizmo PUBLIC ${IMGUIZMO_DIR})
target_link_libraries(imguizmo PRIVATE imgui)

# -----------------------------------------------------------------------------
# nativefiledialog-extended
# -----------------------------------------------------------------------------
add_subdirectory(vendor/nativefiledialog-extended)
