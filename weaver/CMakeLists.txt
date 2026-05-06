project(Weaver)

add_executable(Weaver
  src/main.cpp
  src/editor_layer.h
  src/editor_layer.cpp
  src/panels/scene_hierarchy_panel.cpp
  src/panels/content_browser_panel.cpp
  src/panels/viewport_panel.cpp
  src/panels/toolbar_panel.cpp
  src/editor/editor_history.cpp
  src/editor/scene_manager.cpp
  src/editor/project_manager.cpp
  src/editor/editor_prefs.cpp
  src/scripts/player_controller.cpp
)

target_link_libraries(Weaver PRIVATE Loom nfd)

target_include_directories(Weaver PRIVATE src)

add_custom_target(CopyWeaverAssets ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
  "${CMAKE_SOURCE_DIR}/resources"
  "$<TARGET_FILE_DIR:Weaver>/resources"
  COMMENT "Syncing Weaver resources..."
)

add_dependencies(Weaver CopyWeaverAssets)