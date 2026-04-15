project(Loom)

set(ENGINE_SOURCES
  src/core/application.cpp
  src/core/layer.cpp
  src/core/layer_stack.cpp
  src/core/log.cpp
  src/core/window.cpp
  src/renderer/render_command.cpp
  src/renderer/renderer_api.cpp
)

if (WIN32)
  list(APPEND ENGINE_SOURCES
    src/platform/opengl/opengl_context.cpp
    src/platform/opengl/opengl_renderer_api.cpp
    src/platform/windows/windows_input.cpp
    src/platform/windows/windows_window.cpp
  )
elseif(UNIX AND NOT APPLE)
  list(APPEND ENGINE_SOURCES
    src/platform/linux/linux_input.cpp
    src/platform/linux/linux_window.cpp
  )
endif()

add_library(Loom SHARED ${ENGINE_SOURCES})

target_include_directories(Loom
  PUBLIC
    include
  PRIVATE
    src
)

target_compile_definitions(Loom PRIVATE LOOM_BUILD_DLL)

if (WIN32)
  target_link_libraries(Loom PUBLIC spdlog glad opengl32)
  target_compile_definitions(Loom PUBLIC _UNICODE UNICODE)
else()
  target_link_libraries(Loom PUBLIC spdlog glfw glad)
endif()
