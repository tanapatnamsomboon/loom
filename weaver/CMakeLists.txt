project(Weaver)

add_executable(Weaver
  src/main.cpp
  src/editor_layer.h
  src/editor_layer.cpp
  src/panels/scene_hierarchy_panel.cpp
  src/panels/content_browser_panel.cpp
  src/scripts/player_controller.cpp
)

target_link_libraries(Weaver PRIVATE Loom nfd)

target_include_directories(Weaver PRIVATE src)

add_custom_target(CopyWeaverAssets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
  "${CMAKE_SOURCE_DIR}/assets"
  "$<TARGET_FILE_DIR:Weaver>/assets"
  COMMENT "Syncing Weaver assets..."
)

add_dependencies(Weaver CopyWeaverAssets)