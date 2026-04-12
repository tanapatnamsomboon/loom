project(Loom)

add_library(Loom SHARED
  include/loom/application.h
  include/loom/core.h
  src/engine.cpp
  src/application.cpp
)

target_include_directories(Loom PUBLIC include)

target_compile_definitions(Loom PRIVATE ENGINE_BUILD_DLL)