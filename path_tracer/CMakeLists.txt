# CMake project for path_tracer
cmake_minimum_required(VERSION 3.21)
project(path_tracer)
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
	src/cornell_box.hpp
	src/path_tracer.cpp
	src/path_tracer.hpp
)

# include libfloor base configuration
set(LIBFLOOR_USER 1)
if (WIN32)
	include($ENV{ProgramW6432}/floor/include/floor/libfloor.cmake)
else ()
	include(/opt/floor/include/floor/libfloor.cmake)
endif (WIN32)

## dependencies/libraries/packages
find_package(SDL3_image CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_image::SDL3_image)

# standalone build options
if (BUILD_STANDALONE)
	# TODOs:
	#  * when statically linking libfloor, we need to pull in various system libraries -> how to detect these? should be specified within libfloor.cmake?
	#  * properly specify CMAKE_EXE_LINKER_FLAGS (or better remove it entirely)
	#  * can we only export certain symbols (via --export-dynamic-symbol=<list>) instead of exporting everything via -E?
	#  * similarly, can we get rid of whole-archive linking and use a per-library export symbol list instead?
	target_compile_options(${PROJECT_NAME} PUBLIC -fPIC)
	set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -L${sysroot}/lib")
	set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> -static --pie --eh-frame-hdr -E -L/opt/floor/lib -v -e _dlstart ${sysroot}/lib/ldso_rcrt1.o ${sysroot}/lib/crti.o <OBJECTS> ${sysroot}/lib/crtn.o -o <TARGET> -Bstatic --whole-archive -llzma -lc++ -lc++abi -lunwind --no-whole-archive -Bdynamic <LINK_LIBRARIES> --whole-archive -Bstatic -lxcb-dri2 -lxcb-dri3 -lxcb-glx -lxcb-present -lxcb-randr -lxcb-shm -lxcb-sync -lxcb-xfixes -lX11 -lX11-xcb -lXext -lXcursor -lXfixes -lXi -lXrender -lXrandr -lXss -Bdynamic --no-whole-archive --whole-archive -Bstatic -lc -lc_ldso -Bdynamic --no-whole-archive -L ${clang_rt_lib_path} --whole-archive -Bstatic -l:${clang_rt_lib_name} -Bdynamic --no-whole-archive")
endif (BUILD_STANDALONE)

