cmake_minimum_required(VERSION 3.20)

project(LoomWorkspace)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTS OFF CACHE INTERNAL "")
add_subdirectory(vendor/spdlog)

set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "")
set(GLFW_INSTALL OFF CACHE INTERNAL "")
add_subdirectory(vendor/glfw)

add_subdirectory(vendor/glad)

add_subdirectory(vendor/glm)

add_library(stb STATIC vendor/stb/stb_image.cpp)
target_include_directories(stb PUBLIC vendor/stb)

add_subdirectory(vendor/entt)

add_subdirectory(engine)
add_subdirectory(sandbox)