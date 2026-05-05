project(WeaverRuntime)

add_executable(WeaverRuntime
  src/main.cpp
  src/runtime_layer.h
  src/runtime_layer.cpp
)

target_link_libraries(WeaverRuntime PRIVATE Loom)
target_include_directories(WeaverRuntime PRIVATE src)

add_custom_target(CopyRuntimeAssets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
  "${CMAKE_SOURCE_DIR}/resources"
  "$<TARGET_FILE_DIR:WeaverRuntime>/resources"
  COMMENT "Syncing WeaverRuntime resources..."
)

add_dependencies(WeaverRuntime CopyRuntimeAssets)
