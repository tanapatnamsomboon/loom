project(Loom)

set(ENGINE_SOURCES
  src/core/application.cpp
  src/core/input.cpp
  src/core/layer.cpp
  src/core/layer_stack.cpp
  src/core/log.cpp
  src/core/window.cpp

  src/renderer/render_command.cpp
  src/renderer/renderer_api.cpp

  src/platform/glfw/glfw_input.cpp
  src/platform/glfw/glfw_window.cpp
  src/platform/opengl/opengl_context.cpp
  src/platform/opengl/opengl_renderer_api.cpp
)

if (WIN32)
  list(APPEND ENGINE_SOURCES
    src/platform/windows/windows_input.cpp
    src/platform/windows/windows_window.cpp
  )
endif ()

add_library(Loom SHARED ${ENGINE_SOURCES})

target_include_directories(Loom
  PUBLIC
    include
  PRIVATE
    src
)

target_compile_definitions(Loom PRIVATE LOOM_BUILD_DLL)
target_link_libraries(Loom PUBLIC spdlog glfw glad)

if (WIN32)
  target_compile_definitions(Loom PUBLIC _UNICODE UNICODE)
endif()
