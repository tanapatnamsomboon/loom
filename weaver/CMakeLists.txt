project(Weaver)

add_executable(Weaver
  src/main.cpp
  src/editor_layer.h
  src/editor_layer.cpp
)

target_link_libraries(Weaver PRIVATE Loom)

add_custom_command(TARGET Weaver POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_SOURCE_DIR}/assets"
  $<TARGET_FILE_DIR:Weaver>/assets
)