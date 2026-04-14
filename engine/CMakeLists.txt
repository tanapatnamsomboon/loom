project(Loom)

set(ENGINE_SOURCES
  src/core/application.cpp
  src/core/log.cpp
  src/core/window.cpp
)

if (WIN32)
  list(APPEND ENGINE_SOURCES
    src/platform/windows/windows_window.cpp
  )
elseif(UNIX AND NOT APPLE)
  list(APPEND ENGINE_SOURCES
    src/platform/linux/linux_window.cpp
    src/platform/linux/linux_input.cpp
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
  target_link_libraries(Loom PUBLIC spdlog)
  target_compile_definitions(Loom PUBLIC _UNICODE UNICODE)
else()
  target_link_libraries(Loom PUBLIC spdlog glfw)
endif()
