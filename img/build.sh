#!/bin/sh

# TODO: intro

##########################################
# helper functions
error() {
	printf "\033[1;31m>> $@ \033[m\n"
	exit 1
}
warning() {
	printf "\033[1;33m>> $@ \033[m\n"
}
info() {
	printf "\033[1;32m>> $@ \033[m\n"
}
verbose() {
	if [ ${BUILD_VERBOSE} -gt 0 ]; then
		printf "$@\n"
	fi
}

##########################################
# compiler setup/check

# if no CXX/CC are specified, try using clang++/clang
if [ -z "${CXX}" ]; then
	CXX=clang++
fi
if [ -z "${CC}" ]; then
	CC=clang
fi

# check if clang is the compiler, fail if not
CXX_VERSION=$(${CXX} -v 2>&1)
if expr "${CXX_VERSION}" : ".*clang" >/dev/null; then
	# also check the clang version
	if expr "${CXX_VERSION}" : "Apple.*" >/dev/null; then
		# apple xcode/llvm/clang versioning scheme -> at least 6.1.0 is required (ships with Xcode 6.3)
		if expr "$(echo ${CXX_VERSION} | head -n 1 | sed -E "s/Apple LLVM version ([0-9.]+) .*/\1/")" \< "6.1.0" >/dev/null; then
			error "at least Xcode 6.3 / clang/LLVM 6.1.0 is required to compile this project!"
		fi
	else
		# standard clang versioning scheme -> at least 3.5.0 is required
		if expr "$(echo ${CXX_VERSION} | head -n 1 | sed -E "s/.*clang version ([0-9.]+) .*/\1/")" \< "3.5.0" >/dev/null; then
			error "at least clang 3.5.0 is required to compile this project!"
		fi
	fi
else
	error "only clang is currently supported - please set CXX/CC to clang++/clang and try again!"
fi

##########################################
# arg handling
BUILD_MODE="release"
BUILD_VERBOSE=0
BUILD_JOB_COUNT=0

# read/evaluate floor_conf.hpp to know which build configuration should be used (must match the floor one!)
eval $(printf "" | ${CXX} -E -dM ${INCLUDES} -isystem /usr/include -isystem /usr/local/include -include floor/floor/floor_conf.hpp - 2>&1 | grep -E "define FLOOR_" | sed -E "s/.*define (.*) [\"]*([^ \"]*)[\"]*/export \1=\2/g")
BUILD_CONF_OPENCL=$((1 - $((${FLOOR_NO_OPENCL}))))
BUILD_CONF_CUDA=$((1 - $((${FLOOR_NO_CUDA}))))
BUILD_CONF_OPENAL=$((1 - $((${FLOOR_NO_OPENAL}))))
BUILD_CONF_HOST_COMPUTE=$((1 - $((${FLOOR_NO_HOST_COMPUTE}))))
BUILD_CONF_METAL=$((1 - $((${FLOOR_NO_METAL}))))
BUILD_CONF_NET=$((1 - $((${FLOOR_NO_NET}))))
BUILD_CONF_XML=$((1 - $((${FLOOR_NO_XML}))))
BUILD_CONF_EXCEPTIONS=$((1 - $((${FLOOR_NO_EXCEPTIONS}))))
BUILD_CONF_NO_CL_PROFILING=$((1 - $((${FLOOR_CL_PROFILING}))))
BUILD_CONF_POCL=0
BUILD_CONF_LIBSTDCXX=0

BUILD_ARCH_SIZE="x64"
BUILD_ARCH=$(${CC} -dumpmachine | sed "s/-.*//")
case $BUILD_ARCH in
	"i386"|"i486"|"i586"|"i686"|"arm7"*|"armv7"*)
		BUILD_ARCH_SIZE="x32"
		;;
	"x86_64"|"amd64"|"arm64")
		BUILD_ARCH_SIZE="x64"
		;;
	*)
		warning "unknown architecture (${BUILD_ARCH}) - using ${BUILD_ARCH_SIZE}!"
		;;
esac

# TODO: install/uninstall?
for arg in "$@"; do
	case $arg in
		"help"|"-help"|"--help")
			info "build script usage:"
			echo ""
			echo "build mode options:"
			echo "	<default>          builds this project in release mode"
			echo "	opt                builds this project in release mode + additional optimizations that take longer to compile (lto)"
			echo "	debug              builds this project in debug mode"
			echo "	clean              cleans all build binaries and intermediate build files"
			echo ""
			echo "build configuration:"
			echo "	libstdc++          use libstdc++ instead of libc++ (highly discouraged unless building on mingw)"
			echo "	x32                build a 32-bit binary "$(if [ "${BUILD_ARCH_SIZE}" == "x32" ]; then printf "(default on this platform)"; fi)
			echo "	x64                build a 64-bit binary "$(if [ "${BUILD_ARCH_SIZE}" == "x64" ]; then printf "(default on this platform)"; fi)
			echo ""
			echo "misc flags:"
			echo "	-v                 verbose output (prints all executed compiler and linker commands, and some other information)"
			echo "	-vv                very verbose output (same as -v + runs all compiler and linker commands with -v)"
			echo "	-j#                explicitly use # amount of build jobs (instead of automatically using #logical-cpus jobs)"
			echo ""
			echo ""
			echo "example:"
			echo "	./build.sh -v debug no-cuda no-openal -j1"
			echo ""
			exit 0
			;;
		"opt")
			BUILD_MODE="release_opt"
			;;
		"debug")
			BUILD_MODE="debug"
			;;
		"clean")
			BUILD_MODE="clean"
			;;
		"-v")
			BUILD_VERBOSE=1
			;;
		"-vv")
			BUILD_VERBOSE=2
			;;
		"-j"*)
			BUILD_JOB_COUNT=$(echo $arg | cut -c 3-)
			if [ -z ${BUILD_JOB_COUNT} ]; then
				BUILD_JOB_COUNT=0
			fi
			;;
		"libstdc++")
			BUILD_CONF_LIBSTDCXX=1
			;;
		"x32")
			BUILD_ARCH_SIZE="x32"
			;;
		"x64")
			BUILD_ARCH_SIZE="x64"
			;;
		*)
			warning "unknown argument: ${arg}"
			;;
	esac
done

# sanity check
if [ ${BUILD_CONF_EXCEPTIONS} -eq 0 ]; then
	if [ ${BUILD_CONF_NET} -gt 0 ]; then
		error "when building without exceptions, network support must be disabled as well! (./build.sh no-exceptions no-net)"
	fi
fi

##########################################
# target and build environment setup

# name of the target (part of the binary name)
TARGET_NAME=img

# check on which platform we're compiling + check how many h/w threads can be used (logical cpus)
BUILD_PLATFORM=$(uname | tr [:upper:] [:lower:])
BUILD_OS="unknown"
BUILD_CPU_COUNT=1
case ${BUILD_PLATFORM} in
	"darwin")
		if expr `uname -p` : "arm.*" >/dev/null; then
			BUILD_OS="ios"
		else
			BUILD_OS="osx"
		fi
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"linux")
		BUILD_OS="linux"
		# note that this includes hyper-threading and multi-socket systems
		BUILD_CPU_COUNT=$(cat /proc/cpuinfo | grep "processor" | wc -l)
		;;
	"freebsd")
		BUILD_OS="freebsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"openbsd")
		BUILD_OS="openbsd"
		BUILD_CPU_COUNT=$(sysctl -n hw.ncpu)
		;;
	"cygwin"*)
		# untested
		BUILD_OS="cygwin"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		warning "cygwin support is untested!"
		;;
	"mingw"*)
		BUILD_OS="mingw"
		BUILD_CPU_COUNT=$(env | grep 'NUMBER_OF_PROCESSORS' | sed -E 's/.*=([:digit:]*)/\1/g')
		;;
	*)
		warning "unknown build platform - trying to continue! ${BUILD_PLATFORM}"
		;;
esac

# if an explicit job count was specified, overwrite BUILD_CPU_COUNT with it
if [ ${BUILD_JOB_COUNT} -gt 0 ]; then
	BUILD_CPU_COUNT=${BUILD_JOB_COUNT}
fi

# set the target binary name (depends on the platform)
TARGET_BIN_NAME=${TARGET_NAME}
# append 'd' for debug builds
if [ $BUILD_MODE == "debug" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}d
fi

# use *.a for all platforms
TARGET_STATIC_BIN_NAME=${TARGET_BIN_NAME}.a

# file ending, depending on the platform we're building on
# windows/mingw/cygwin -> .exe
if [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
	TARGET_BIN_NAME=${TARGET_BIN_NAME}.exe
fi
# all else: no file ending

# disable metal support on non-iOS/-OS X targets
if [ $BUILD_OS != "osx" -a $BUILD_OS != "ios" ]; then
	BUILD_CONF_METAL=0
fi

##########################################
# directory setup
# note that all paths are relative

# binary/library directory where the final binaries will be stored (*.so, *.dylib, etc.)
BIN_DIR=bin

# location of the target binary
TARGET_BIN=${BIN_DIR}/${TARGET_BIN_NAME}
TARGET_STATIC_BIN=${BIN_DIR}/${TARGET_STATIC_BIN_NAME}

# root folder of the source code
SRC_DIR="src"

# all source code sub-directories, relative to SRC_DIR
SRC_SUB_DIRS="."

# build directory where all temporary files are stored (*.o, etc.)
BUILD_DIR=build

##########################################
# library/dependency handling

# initial linker, lib and include setup
LDFLAGS="${LDFLAGS} -fvisibility=default"
if [ ${BUILD_CONF_LIBSTDCXX} -gt 0 ]; then
	LDFLAGS="${LDFLAGS} -stdlib=libstdc++"
else
	LDFLAGS="${LDFLAGS} -stdlib=libc++"
fi
LIBS="${LIBS}"
INCLUDES="${INCLUDES} -isystem /usr/local/include/c++/v1"
COMMON_FLAGS="${COMMON_FLAGS}"

# if no AR is specified, set it to the default ar (used when creating a static lib)
if [ -z "${AR}" ]; then
	AR=ar
fi

# set the correct 32/64-bit linker flag (use the default on mingw)
if [ $BUILD_OS != "mingw" ]; then
	if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
		LDFLAGS="${LDFLAGS} -m32"
	else
		LDFLAGS="${LDFLAGS} -m64"
	fi
fi

# link against floor (note: floor debug lib is suffixed by "d")
if [ $BUILD_MODE == "debug" ]; then
	if [ $BUILD_OS == "mingw" ]; then
		LDFLAGS="${LDFLAGS} -lfloord_static"
	else
		LDFLAGS="${LDFLAGS} -lfloord"
	fi
else
	if [ $BUILD_OS == "mingw" ]; then
		LDFLAGS="${LDFLAGS} -lfloor_static"
	else
		LDFLAGS="${LDFLAGS} -lfloor"
	fi
fi

# use pkg-config (and some manual libs/includes) on all platforms except osx/ios
if [ $BUILD_OS != "osx" -a $BUILD_OS != "ios" ]; then
	# need to make kernel symbols visible for dlsym
	LDFLAGS="${LDFLAGS} -rdynamic"
	
	# use PIC
	LDFLAGS="${LDFLAGS} -fPIC"
	COMMON_FLAGS="${COMMON_FLAGS} -fPIC"
	
	# pkg-config: required libraries/packages and optional libraries/packages
	PACKAGES="sdl2"
	PACKAGES_OPT=""
	if [ ${BUILD_CONF_XML} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} libxml-2.0"
	fi
	if [ ${BUILD_CONF_NET} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} libcrypto libssl"
	fi
	if [ ${BUILD_CONF_OPENAL} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} openal"
	fi
	if [ ${BUILD_CONF_POCL} -gt 0 ]; then
		PACKAGES_OPT="${PACKAGES_OPT} pocl"
	fi

	# TODO: error checking + check if libs exist
	for pkg in ${PACKAGES}; do
		LIBS="${LIBS} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done
	for pkg in ${PACKAGES_OPT}; do
		LIBS="${LIBS} $(pkg-config --libs "${pkg}")"
		COMMON_FLAGS="${COMMON_FLAGS} $(pkg-config --cflags "${pkg}")"
	done

	# libs that don't have pkg-config
	UNCHECKED_LIBS=""
	if [ $BUILD_OS != "mingw" ]; then
		# must link pthread on unix
		UNCHECKED_LIBS="${UNCHECKED_LIBS} pthread"
	else
		# must link winpthread instead on windows/mingw
		UNCHECKED_LIBS="${UNCHECKED_LIBS} winpthread"
	fi
	if [ ${BUILD_CONF_OPENCL} -gt 0 -a ${BUILD_CONF_POCL} -eq 0 ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} OpenCL"
	fi
	if [ ${BUILD_CONF_CUDA} -gt 0 ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} cuda"
	fi

	# add os specific libs
	if [ $BUILD_OS == "linux" -o $BUILD_OS == "freebsd" -o $BUILD_OS == "openbsd" ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} GL Xxf86vm"
	elif [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
		UNCHECKED_LIBS="${UNCHECKED_LIBS} opengl32 glu32 gdi32 ws2_32"
	fi
	
	# linux:
	#  * must also link against c++abi when using libc++
	#  * need to add the /lib folder
	if [ $BUILD_OS == "linux" ]; then
		if [ ${BUILD_CONF_LIBSTDCXX} -eq 0 ]; then
			UNCHECKED_LIBS="${UNCHECKED_LIBS} c++abi"
		fi
		LDFLAGS="${LDFLAGS} -L/lib"
	fi
	
	# windows/mingw opencl and cuda handling
	if [ $BUILD_OS == "mingw" ]; then
		if [ ${BUILD_CONF_OPENCL} -gt 0 ]; then
			if [ ! -z "${AMDAPPSDKROOT}" ]; then
				# use amd opencl sdk
				AMDAPPSDKROOT_FIXED=$(echo ${AMDAPPSDKROOT} | sed -E "s/\\\\/\//g")
				if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
					LDFLAGS="${LDFLAGS} -L\"${AMDAPPSDKROOT_FIXED}lib/x86\""
				else
					LDFLAGS="${LDFLAGS} -L\"${AMDAPPSDKROOT_FIXED}lib/x86_64\""
				fi
				INCLUDES="${INCLUDES} -isystem \"${AMDAPPSDKROOT_FIXED}include\""
			elif [ ! -z "${INTELOCLSDKROOT}" ]; then
				# use intel opencl sdk
				INTELOCLSDKROOT_FIXED=$(echo ${INTELOCLSDKROOT} | sed -E "s/\\\\/\//g")
				if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
					LDFLAGS="${LDFLAGS} -L\"${INTELOCLSDKROOT_FIXED}lib/x86\""
				else
					LDFLAGS="${LDFLAGS} -L\"${INTELOCLSDKROOT_FIXED}lib/x64\""
				fi
				INCLUDES="${INCLUDES} -isystem \"${INTELOCLSDKROOT_FIXED}include\""
			elif [ ! -z "${CUDA_PATH}" ]; then
				# use nvidia opencl/cuda sdk
				CUDA_PATH_FIXED=$(echo ${CUDA_PATH} | sed -E "s/\\\\/\//g")
				if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
					LDFLAGS="${LDFLAGS} -L\"${CUDA_PATH_FIXED}/lib/Win32\""
				else
					LDFLAGS="${LDFLAGS} -L\"${CUDA_PATH_FIXED}/lib/x64\""
				fi
				INCLUDES="${INCLUDES} -isystem \"${CUDA_PATH_FIXED}/include\""
			else
				error "building with OpenCL support, but no OpenCL SDK was found - please install the Intel, AMD or NVIDIA OpenCL SDK!"
			fi
		fi
		
		if [ ${BUILD_CONF_CUDA} -gt 0 ]; then
			CUDA_PATH_FIXED=$(echo ${CUDA_PATH} | sed -E "s/\\\\/\//g")
			if [ -z "${CUDA_PATH}" ]; then
				error "building with CUDA support, but CUDA is not installed or CUDA_PATH is not set!"
			fi
			if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
				LDFLAGS="${LDFLAGS} -L\"${CUDA_PATH_FIXED}/lib/Win32\""
			else
				LDFLAGS="${LDFLAGS} -L\"${CUDA_PATH_FIXED}/lib/x64\""
			fi
			INCLUDES="${INCLUDES} -isystem \"${CUDA_PATH_FIXED}/include\""
		fi
	fi

	for lib in ${UNCHECKED_LIBS}; do
		LIBS="${LIBS} -l${lib}"
	done
	
	# mingw: "--allow-multiple-definition" is necessary, because gcc is still used as a linker
	# and will always link against libstdc++/libsupc++ (-> multiple definitions with libc++)
	# also note: since libc++ is linked first, libc++'s functions will be used
	if [ $BUILD_OS == "mingw" -a ${BUILD_CONF_LIBSTDCXX} -eq 0 ]; then
		LDFLAGS="${LDFLAGS} -lc++.dll -Wl,--allow-multiple-definition -lsupc++"
	fi
	
	# add all libs to LDFLAGS
	LDFLAGS="${LDFLAGS} ${LIBS}"
else
	# on osx/ios: assume everything is installed, pkg-config doesn't really exist
	INCLUDES="${INCLUDES} -isystem /opt/X11/include"
	if [ ${BUILD_CONF_XML} -gt 0 ]; then
		INCLUDES="${INCLUDES} -isystem /usr/include/libxml2"
	fi
	if [ ${BUILD_CONF_NET} -gt 0 ]; then
		INCLUDES="${INCLUDES} -isystem /usr/local/opt/openssl/include"
	fi
	INCLUDES="${INCLUDES} -iframework /Library/Frameworks"
	
	# additional lib paths
	LDFLAGS="${LDFLAGS} -L/opt/X11/lib"
	if [ ${BUILD_CONF_NET} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -L/usr/local/opt/openssl/lib"
	fi

	# rpath voodoo
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/local/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /usr/lib"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Resources"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker @loader_path/../Frameworks"
	LDFLAGS="${LDFLAGS} -Xlinker -rpath -Xlinker /Library/Frameworks"
	
	# probably necessary
	LDFLAGS="${LDFLAGS} -fobjc-link-runtime"
	
	# frameworks and libs
	LDFLAGS="${LDFLAGS} -framework SDL2"
	if [ ${BUILD_CONF_XML} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -lxml2"
	fi
	if [ ${BUILD_CONF_NET} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -lcrypto -lssl"
	fi
	if [ ${BUILD_CONF_CUDA} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -weak_framework CUDA"
	fi
	if [ ${BUILD_CONF_OPENAL} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -framework OpenALSoft"
	fi
	
	# system frameworks
	LDFLAGS="${LDFLAGS} -framework ApplicationServices -framework AppKit -framework Cocoa -framework OpenGL"
	if [ ${BUILD_CONF_OPENCL} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -framework OpenCL"
	fi
	if [ ${BUILD_CONF_METAL} -gt 0 ]; then
		LDFLAGS="${LDFLAGS} -framework Metal"
	fi
fi

# just in case, also add these rather default ones (should also go after all previous libs/includes,
# in case a local or otherwise set up lib is overwriting a system lib and should be used instead)
LDFLAGS="${LDFLAGS} -L/usr/lib -L/usr/local/lib"
INCLUDES="${INCLUDES}"
# don't automatically add /usr/include and /usr/local/include on mingw/msys (these will lead to the wrong headers being included)
if [ $BUILD_OS != "mingw" ]; then
    INCLUDES="${INCLUDES} -isystem /usr/include -isystem /usr/local/include"
fi

# on linux: need to explicitly add the cuda lib + include folder and must do this _after_ /usr/{include,lib},
# because its obsolete CL folder and lib conflict with the system ones
if [ $BUILD_OS == "linux" ]; then
	if [ ${BUILD_CONF_CUDA} -gt 0 ]; then
		INCLUDES="${INCLUDES} -isystem /opt/cuda/include"
		if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
			LDFLAGS="${LDFLAGS} -L/opt/cuda/lib"
		else
			LDFLAGS="${LDFLAGS} -L/opt/cuda/lib64"
		fi
	fi
fi

##########################################
# flags

# set up initial c++ and c flags
CXXFLAGS="${CXXFLAGS} -std=gnu++14"
if [ ${BUILD_CONF_LIBSTDCXX} -gt 0 ]; then
	CXXFLAGS="${CXXFLAGS} -stdlib=libstdc++"
else
	CXXFLAGS="${CXXFLAGS} -stdlib=libc++"
fi
CFLAGS="${CFLAGS} -std=gnu11"

# disable exception support
if [ ${BUILD_CONF_EXCEPTIONS} -eq 0 ]; then
	CXXFLAGS="${CXXFLAGS} -fno-exceptions"
fi

# arch handling (use -arch on osx/ios and -m32/-m64 everywhere else, except for mingw)
if [ $BUILD_OS == "osx" -o $BUILD_OS == "ios" ]; then
	if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
		case $BUILD_ARCH in
			"i386"|"i486"|"i586"|"i686")
				COMMON_FLAGS="${COMMON_FLAGS} -arch $BUILD_ARCH"
				;;
			"x86_64"|"amd64")
				COMMON_FLAGS="${COMMON_FLAGS} -arch i686"
				;;
			"arm"*)
				COMMON_FLAGS="${COMMON_FLAGS} -arch armv7"
				;;
			*)
				warning "unknown arch (${BUILD_ARCH}) - building for arch i686!"
				COMMON_FLAGS="${COMMON_FLAGS} -arch i686"
				;;
		esac
	else
		case $BUILD_ARCH in
			"arm"*)
				COMMON_FLAGS="${COMMON_FLAGS} -arch arm64"
				;;
			*)
				COMMON_FLAGS="${COMMON_FLAGS} -arch x86_64"
				;;
		esac
	fi
elif [ $BUILD_OS != "mingw" ]; then
	# NOTE: mingw will/should/has to use the compiler default
	if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
		COMMON_FLAGS="${COMMON_FLAGS} -m32"
	else
		COMMON_FLAGS="${COMMON_FLAGS} -m64"
	fi
fi

# c++ and c flags that apply to all build configurations
COMMON_FLAGS="${COMMON_FLAGS} -ffast-math -fstrict-aliasing"

# debug flags, only used in the debug target
DEBUG_FLAGS="-O0 -DFLOOR_DEBUG=1 -DDEBUG"
if [ $BUILD_OS != "mingw" ]; then
	DEBUG_FLAGS="${DEBUG_FLAGS} -gdwarf-2"
else
	DEBUG_FLAGS="${DEBUG_FLAGS} -g"
fi

# release mode flags/optimizations
# TODO: sse/avx selection/config? default to sse4.1 for now (core2)
# TODO: also add -mtune option
REL_FLAGS="-Ofast -funroll-loops -msse4.1"

# additional optimizations (used in addition to REL_CXX_FLAGS)
REL_OPT_FLAGS="-flto"
REL_OPT_LD_FLAGS="-flto"

# osx/ios: set min version
if [ $BUILD_OS == "osx" -o $BUILD_OS == "ios" ]; then
	if [ $BUILD_OS == "osx" ]; then
		COMMON_FLAGS="${COMMON_FLAGS} -mmacosx-version-min=10.9"
	else
		COMMON_FLAGS="${COMMON_FLAGS} -miphoneos-version-min=8.0"
	fi
fi

# defines:
# set platform size define
if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
	COMMON_FLAGS="${COMMON_FLAGS} -DPLATFORM_X32"
else
	COMMON_FLAGS="${COMMON_FLAGS} -DPLATFORM_X64"
fi
if [ $BUILD_OS == "mingw" -o $BUILD_OS == "cygwin" ]; then
	# common windows "unix environment" flag
	COMMON_FLAGS="${COMMON_FLAGS} -DWIN_UNIXENV"
	if [ $BUILD_OS == "mingw" ]; then
		# set __WINDOWS__ and mingw specific flag
		COMMON_FLAGS="${COMMON_FLAGS} -D__WINDOWS__ -DMINGW"
		# tell boost to use windows.h (will get conflicts otherwise)
		COMMON_FLAGS="${COMMON_FLAGS} -DBOOST_USE_WINDOWS_H"
	fi
	if [ $BUILD_OS == "cygwin" ]; then
		# set cygwin specific flag
		COMMON_FLAGS="${COMMON_FLAGS} -DCYGWIN"
	fi
fi

# hard-mode c++ ;) TODO: clean this up + explanations
WARNINGS="${WARNINGS} -Weverything -Wthread-safety -Wthread-safety-negative -Wthread-safety-beta -Wthread-safety-verbose"
WARNINGS="${WARNINGS} -Wno-gnu -Wno-c++98-compat"
WARNINGS="${WARNINGS} -Wno-c++98-compat-pedantic -Wno-c99-extensions"
WARNINGS="${WARNINGS} -Wno-header-hygiene -Wno-documentation"
WARNINGS="${WARNINGS} -Wno-system-headers -Wno-global-constructors -Wno-padded"
WARNINGS="${WARNINGS} -Wno-packed -Wno-switch-enum -Wno-exit-time-destructors"
WARNINGS="${WARNINGS} -Wno-unknown-warning-option -Wno-nested-anon-types"
WARNINGS="${WARNINGS} -Wno-old-style-cast -Wno-date-time -Wno-reserved-id-macro"
WARNINGS="${WARNINGS} -Wno-documentation-unknown-command"
if [ ${BUILD_ARCH_SIZE} == "x32" ]; then
	# ignore warnings about required alignment increases on 32-bit platforms (won't and can't fix)
	WARNINGS="${WARNINGS} -Wno-cast-align"
fi
COMMON_FLAGS="${COMMON_FLAGS} ${WARNINGS}"

# diagnostics
COMMON_FLAGS="${COMMON_FLAGS} -fdiagnostics-show-note-include-stack -fmessage-length=0 -fmacro-backtrace-limit=0"
COMMON_FLAGS="${COMMON_FLAGS} -fparse-all-comments -fno-elide-type -fdiagnostics-show-template-tree"

# includes + replace all "-I"s with "-isystem"s so that we don't get warnings in external headers
COMMON_FLAGS="${COMMON_FLAGS} ${INCLUDES}"
COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-I/-isystem /g")
COMMON_FLAGS="${COMMON_FLAGS} -I${SRC_DIR}"

# mingw sdl fixes
if [ $BUILD_OS == "mingw" ]; then
	# remove sdls main redirect, we want to use our own main
	COMMON_FLAGS=$(echo "${COMMON_FLAGS}" | sed -E "s/-Dmain=SDL_main//g")
	# remove windows flag -> creates a separate cmd window + working iostream output
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-mwindows //g")
	# remove unwanted -static-libgcc flag (this won't work and lead to linker errors!)
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-static-libgcc //g")
	# remove unwanted -lm (this won't work and lead to linker errors!)
	LDFLAGS=$(echo "${LDFLAGS}" | sed -E "s/-lm //g")
fi

# finally: add all common c++ and c flags/options
CXXFLAGS="${CXXFLAGS} ${COMMON_FLAGS}"
CFLAGS="${CFLAGS} ${COMMON_FLAGS}"

##########################################
# targets and building

# get all source files (c++/c/objective-c++/objective-c) and create build folders
for dir in ${SRC_SUB_DIRS}; do
	# hanlde paths correctly (don't want /./ or //)
	if [ ${dir} == "." ]; then
		dir=""
	else
		dir="/${dir}"
	fi

	# source files
	SRC_FILES="${SRC_FILES} $(find ${SRC_DIR}${dir} -maxdepth 1 -type f -name '*.cpp')"
	SRC_FILES="${SRC_FILES} $(find ${SRC_DIR}${dir} -maxdepth 1 -type f -name '*.c')"
	SRC_FILES="${SRC_FILES} $(find ${SRC_DIR}${dir} -maxdepth 1 -type f -name '*.mm')"
	SRC_FILES="${SRC_FILES} $(find ${SRC_DIR}${dir} -maxdepth 1 -type f -name '*.m')"
	
	# create resp. build folder
	mkdir -p ${BUILD_DIR}/${SRC_DIR}${dir}
done

# make a list of all object files
for source_file in ${SRC_FILES}; do
	OBJ_FILES="${OBJ_FILES} ${BUILD_DIR}/${source_file}.o"
done

# set flags depending on the build mode, or make a clean exit
case ${BUILD_MODE} in
	"release")
		# release mode (default): add release mode flags/optimizations
		CXXFLAGS="${CXXFLAGS} ${REL_FLAGS}"
		CFLAGS="${CFLAGS} ${REL_FLAGS}"
		;;
	"release_opt")
		# release mode + additional optimizations: add release mode and opt flags
		CXXFLAGS="${CXXFLAGS} ${REL_FLAGS} ${REL_OPT_FLAGS}"
		CFLAGS="${CFLAGS} ${REL_FLAGS} ${REL_OPT_FLAGS}"
		LDFLAGS="${LDFLAGS} ${REL_OPT_LD_FLAGS}"
		;;
	"debug")
		# debug mode: add debug flags
		CXXFLAGS="${CXXFLAGS} ${DEBUG_FLAGS}"
		CFLAGS="${CFLAGS} ${DEBUG_FLAGS}"
		;;
	"clean")
		# delete the target binary and the complete build folder (all object files)
		info "cleaning ..."
		rm -f ${TARGET_BIN}
		rm -Rf ${BUILD_DIR}
		exit 0
		;;
	*)
		error "unknown build mode: ${BUILD_MODE}"
		;;
esac

if [ ${BUILD_VERBOSE} -gt 1 ]; then
	CXXFLAGS="${CXXFLAGS} -v"
	CFLAGS="${CFLAGS} -v"
	LDFLAGS="${LDFLAGS} -v"
fi
if [ ${BUILD_VERBOSE} -gt 0 ]; then
	info ""
	info "using CXXFLAGS: ${CXXFLAGS}"
	info ""
	info "using CFLAGS: ${CFLAGS}"
	info ""
	info "using LDFLAGS: ${LDFLAGS}"
	info ""
fi

# build the target
build_file() {
	# this function builds one source file
	source_file=$1
	file_num=$2
	file_count=$3
	info "building ${source_file} [${file_num}/${file_count}]"
	case ${source_file} in
		*".cpp")
			build_cmd="${CXX} ${CXXFLAGS}"
			;;
		*".c")
			build_cmd="${CC} ${CFLAGS}"
			;;
		*".mm")
			build_cmd="${CXX} -x objective-c++ ${CXXFLAGS}"
			;;
		*".m")
			build_cmd="${CC} -x objective-c ${CFLAGS}"
			;;
		*)
			error "unknown source file ending: ${source_file}"
			;;
	esac
	build_cmd="${build_cmd} -c ${source_file} -o ${BUILD_DIR}/${source_file}.o -MMD -MT deps -MF ${BUILD_DIR}/${source_file}.d"
	verbose "${build_cmd}"
	eval ${build_cmd}
}
job_count() {
	echo $(jobs -p | wc -l)
}

# get the amount of source files and create a counter (this is used for some info/debug output)
file_count=$(echo "${SRC_FILES}" | wc -w | tr -d [:space:])
file_counter=0
# iterate over all source files and create a build job for each of them
for source_file in ${SRC_FILES}; do
	file_counter=$(expr $file_counter + 1)
	
	# if only one build job should be used, don't bother with shell jobs
	# this also works around an issue where "jobs -p" always lists one job even after it has finished
	if [ $BUILD_CPU_COUNT -gt 1 ]; then
		# make sure that there are only $BUILD_CPU_COUNT active jobs at any time,
		# this should be the most efficient setup for concurrently building multiple files
		while true; do
			cur_job_count=$(job_count)
			if [ $cur_job_count -lt $BUILD_CPU_COUNT ]; then
				break
			fi
			sleep 0.1
		done
		(build_file $source_file $file_counter $file_count) &
	else
		build_file $source_file $file_counter $file_count
	fi
done
# all jobs were started, now we just have to wait until all are done
sleep 0.1
info "waiting for build jobs to finish ..."
wait

# link
info "linking ..."
mkdir -p ${BIN_DIR}

linker_cmd="${CXX} -o ${TARGET_BIN} ${OBJ_FILES} ${LDFLAGS}"
verbose "${linker_cmd}"
eval ${linker_cmd}

info "built ${TARGET_NAME} v${TARGET_FULL_VERSION}"
