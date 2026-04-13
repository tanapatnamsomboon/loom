project(Loom)

add_library(Loom SHARED
  include/loom/core/application.h
  include/loom/core/core.h
  include/loom/core/log.h
  include/loom/events/event.h
  src/core/application.cpp
  src/core/log.cpp
)

target_include_directories(Loom PUBLIC include)
target_compile_definitions(Loom PRIVATE ENGINE_BUILD_DLL)

target_link_libraries(Loom PUBLIC spdlog)