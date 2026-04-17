project(Sandbox)

add_executable(Sandbox
  src/main.cpp
)

target_link_libraries(Sandbox PRIVATE Loom)

add_custom_command(TARGET Sandbox POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_SOURCE_DIR}/assets"
  $<TARGET_FILE_DIR:Sandbox>/assets
)