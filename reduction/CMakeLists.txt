# CMake project for reduction
cmake_minimum_required(VERSION 3.21)
project(reduction)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## build options
option(WITH_ASAN "build with address sanitizer" OFF)
option(WITH_LIBCXX "build with libc++" OFF)
option(BUILD_STANDALONE "build as a standalone binary (requires toolchain)" OFF)

## build output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## source files
include_directories("src")
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/reduction_state.hpp
	src/reduction.cpp
	src/reduction.hpp
)

# include libfloor base configuration
set(LIBFLOOR_USER 1)
if (WIN32)
	include($ENV{ProgramW6432}/floor/include/floor/libfloor.cmake)
else ()
	include(/opt/floor/include/floor/libfloor.cmake)
endif (WIN32)

# standalone build options
if (BUILD_STANDALONE)
	# TODO
endif (BUILD_STANDALONE)
