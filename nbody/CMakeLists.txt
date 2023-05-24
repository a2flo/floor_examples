# VS/vcpkg/clang/CMake project for nbody
cmake_minimum_required(VERSION 3.11)
project(nbody)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## build options
set(WITH_ASAN false CACHE BOOL "build with address sanitizer")

## build output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## source files
include_directories("src")
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/nbody.cpp
	src/nbody.hpp
	src/nbody_state.hpp
	src/gl_renderer.cpp
	src/gl_renderer.hpp
	src/vulkan_renderer.cpp
	src/vulkan_renderer.hpp
	src/unified_renderer.cpp
	src/unified_renderer.hpp
)

# include libfloor base configuration
set(LIBFLOOR_USER 1)
if (WIN32)
	include($ENV{ProgramW6432}/floor/include/floor/libfloor.cmake)
else ()
	include(/opt/floor/include/floor/libfloor.cmake)
endif (WIN32)
