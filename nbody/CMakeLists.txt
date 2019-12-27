# VS/vcpkg/clang/CMake project for nbody
cmake_minimum_required(VERSION 3.8)
project(nbody)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

## compile flags
add_compile_definitions(_ENABLE_EXTENDED_ALIGNED_STORAGE)
add_compile_options("$<$<CONFIG:DEBUG>:-DFLOOR_DEBUG>")

if (MSVC)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc ")
	add_compile_options("$<$<CONFIG:DEBUG>:-gcodeview>")
	add_compile_options("$<$<CONFIG:DEBUG>:/Zi>")
endif (MSVC)

if (UNIX OR MINGW)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
endif (UNIX OR MINGW)

if (WIN32)
	add_compile_definitions(__WINDOWS__)
	if (MINGW)
		add_compile_definitions(MINGW)
	endif (MINGW)
endif (WIN32)

add_compile_options("$<$<CONFIG:RELEASE>:-Ofast>")
add_compile_options("$<$<CONFIG:RELEASE>:-msse4.1>")
add_compile_options("$<$<CONFIG:RELEASE>:-fstrict-aliasing>")
if (UNIX OR MINGW)
	add_compile_options("$<$<CONFIG:RELEASE>:-ffast-math>")
	add_compile_options("$<$<CONFIG:RELEASE>:-funroll-loops>")
endif (UNIX OR MINGW)
if (MSVC)
	add_compile_options("$<$<CONFIG:RELEASE>:/fp:fast>")
endif (MSVC)

## warnings
# let's start with everything
add_compile_options(-Weverything)
# in case we're using warning options that aren't supported by other clang versions
add_compile_options(-Wno-unknown-warning-option)
# remove std compat warnings (c++17 with gnu and clang extensions is required)
add_compile_options(-Wno-c++98-compat -Wno-c++98-compat-pedantic)
add_compile_options(-Wno-c++11-compat -Wno-c++11-compat-pedantic)
add_compile_options(-Wno-c++14-compat -Wno-c++14-compat-pedantic)
add_compile_options(-Wno-c99-extensions -Wno-c11-extensions)
add_compile_options(-Wno-gnu -Wno-gcc-compat)
# don't be too pedantic
add_compile_options(-Wno-header-hygiene -Wno-documentation -Wno-documentation-unknown-command -Wno-old-style-cast)
add_compile_options(-Wno-global-constructors -Wno-exit-time-destructors -Wno-reserved-id-macro -Wno-date-time)
# suppress warnings in system headers
add_compile_options(-Wno-system-headers)
# these two are only useful in certain situations, but are quite noisy
add_compile_options(-Wno-packed -Wno-padded)
# this conflicts with the other switch/case warning
add_compile_options(-Wno-switch-enum)
# quite useful feature/extension
add_compile_options(-Wno-nested-anon-types)
# this should be taken care of in a different way
add_compile_options(-Wno-partial-availability)
# enable thread-safety warnings
add_compile_options(-Wthread-safety -Wthread-safety-negative -Wthread-safety-beta -Wthread-safety-verbose)
# ignore "explicit move to avoid copying on older compilers" warning
add_compile_options(-Wno-return-std-move-in-c++11)
# diagnostics
add_compile_options(-fmacro-backtrace-limit=0)
# on MinGW, ignore casing warnings (there is an issue where this warnings triggers even if the casing is correct)
if (MINGW)
	add_compile_options("-Wno-nonportable-system-include-path")
endif (MINGW)

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

## output postfix
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "d")

# MinGW: use lld
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE -fuse-ld=lld)
endif (MINGW)

## use console subsystem (MSVC)
if (MSVC)
	set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
	add_compile_definitions(_CONSOLE)
endif (MSVC)

## dependencies/libraries/packages
find_package(SDL2 CONFIG REQUIRED)
if (MSVC)
	target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
endif (MSVC)
if (UNIX OR MINGW)
	target_link_libraries(${PROJECT_NAME} PRIVATE SDL2 SDL2main)
	if (MINGW)
		# MinGW: use the normal main instead of the SDL one
		target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED)
	endif (MINGW)
endif (UNIX OR MINGW)

find_package(OpenGL REQUIRED)
find_path(OPENGL_ARB_INCLUDE_DIR GL/glcorearb.h)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OPENGL_ARB_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL32)

find_package(OpenCL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCL_LIBRARIES})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${OpenCL_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)

find_path(ASIO_INCLUDE_DIR asio.hpp)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${ASIO_INCLUDE_DIR})

find_package(OpenAL CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)

if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE $ENV{VK_SDK_PATH}/Lib)
	target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
endif (MSVC)
if (MINGW)
	target_link_options(${PROJECT_NAME} PRIVATE "$ENV{VK_SDK_PATH}/Lib/vulkan-1.lib")
endif (MINGW)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE $ENV{VK_SDK_PATH}/include)

## add libfloor
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/floor/include)
if (MSVC)
	target_link_directories(${PROJECT_NAME} PRIVATE $ENV{ProgramW6432}/floor/lib)
	target_link_libraries(${PROJECT_NAME} PRIVATE debug libfloord optimized libfloor)
endif (MSVC)
if (MINGW)
	if (CMAKE_BUILD_TYPE MATCHES "DEBUG" OR CMAKE_BUILD_TYPE MATCHES "Debug")
		target_link_options(${PROJECT_NAME} PRIVATE "$ENV{ProgramW6432}/floor/lib/libfloord.dll.a")
	else ()
		target_link_options(${PROJECT_NAME} PRIVATE "$ENV{ProgramW6432}/floor/lib/libfloor.dll.a")
	endif ()
endif (MINGW)
