cmake_minimum_required(VERSION 3.20)

project(LoomWorkspace)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/vendors.cmake)

add_subdirectory(engine)
# add_subdirectory(sandbox)
add_subdirectory(weaver)
