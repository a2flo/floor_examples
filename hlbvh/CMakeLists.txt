# VS/vcpkg/clang/CMake project for hlbvh
cmake_minimum_required(VERSION 3.11)
project(hlbvh)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## build output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

## source files
include_directories("src" "../common/obj" "../common/camera")
add_executable(${PROJECT_NAME}
	src/main.cpp
	src/collider.cpp
	src/collider.hpp
	src/gl_renderer.cpp
	src/gl_renderer.hpp
	src/unified_renderer.cpp
	src/unified_renderer.hpp
	src/animation.cpp
	src/animation.hpp
	src/hlbvh.cpp
	src/hlbvh_shaders.cpp
	src/hlbvh_state.hpp
	src/triangle_intersection.hpp
	../common/obj/obj_loader.cpp
	../common/obj/obj_loader.hpp
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

## dependencies/libraries/packages
find_package(SDL2_image CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2_image::SDL2_image)
