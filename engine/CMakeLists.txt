project(Loom)

set(ENGINE_SOURCES
  src/core/application.cpp
  src/core/input.cpp
  src/core/layer.cpp
  src/core/layer_stack.cpp
  src/core/log.cpp
  src/core/window.cpp

  src/imgui/imgui_layer.cpp

  src/renderer/buffer.cpp
  src/renderer/render_command.cpp
  src/renderer/orthographic_camera.cpp
  src/renderer/renderer_api.cpp
  src/renderer/shader.cpp
  src/renderer/texture.cpp
  src/renderer/vertex_array.cpp

  src/platform/glfw/glfw_input.cpp
  src/platform/glfw/glfw_window.cpp

  src/platform/opengl/opengl_buffer.cpp
  src/platform/opengl/opengl_context.cpp
  src/platform/opengl/opengl_renderer_api.cpp
  src/platform/opengl/opengl_shader.cpp
  src/platform/opengl/opengl_texture.cpp
  src/platform/opengl/opengl_vertex_array.cpp
)

if (WIN32)
  list(APPEND ENGINE_SOURCES
    src/platform/windows/windows_input.cpp
    src/platform/windows/windows_window.cpp
  )
endif ()

set (IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)

set (IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PRIVATE glfw glad)

add_library(Loom SHARED ${ENGINE_SOURCES})

target_include_directories(Loom
  PUBLIC
    include
  PRIVATE
    src
)
target_compile_definitions(Loom PRIVATE LOOM_BUILD_DLL)
target_link_libraries(Loom
  PUBLIC
  spdlog
  glfw
  glad
  imgui
  glm::glm-header-only
  stb
)

if (WIN32)
  target_compile_definitions(Loom PUBLIC _UNICODE UNICODE)
endif()
