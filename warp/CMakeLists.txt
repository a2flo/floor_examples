# CMake project for warp
cmake_minimum_required(VERSION 3.21)
project(warp)
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
include_directories("src" "../common/obj_loader" "../common/camera")
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/warp_shaders.cpp
	src/warp_state.hpp
	src/auto_cam.cpp
	src/auto_cam.hpp
	src/unified_renderer.cpp
	src/unified_renderer.hpp
	../common/obj_loader/obj_loader.cpp
	../common/obj_loader/obj_loader.hpp
	../common/camera/camera.cpp
	../common/camera/camera.hpp
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

## dependencies/libraries/packages
if (NOT BUILD_STANDALONE)
	find_package(SDL3_image CONFIG REQUIRED)
	target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_image::SDL3_image)
endif (NOT BUILD_STANDALONE)

## add libwarp
if (WIN32)
	target_link_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/libwarp/lib)
	target_include_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/libwarp/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE debug libwarpd optimized libwarp)
else ()
	target_link_directories(${PROJECT_NAME} PRIVATE /opt/libwarp/lib)
	target_include_directories(${PROJECT_NAME} PRIVATE /opt/libwarp/include)
	if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
		target_link_options(${PROJECT_NAME} PRIVATE -lwarpd)
	else ()
		target_link_options(${PROJECT_NAME} PRIVATE -lwarp)
	endif ()
endif (WIN32)
