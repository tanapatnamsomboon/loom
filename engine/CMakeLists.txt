project(Engine)

add_library(Engine SHARED
  src/engine.cpp
)

target_include_directories(Engine PUBLIC include)