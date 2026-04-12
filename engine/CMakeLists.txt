project(Loom)

add_library(Loom SHARED
  include/loom/core/application.h
  include/loom/core/core.h
  src/core/application.cpp
)

target_include_directories(Loom PUBLIC include)

target_compile_definitions(Loom PRIVATE ENGINE_BUILD_DLL)