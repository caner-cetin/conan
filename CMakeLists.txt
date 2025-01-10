# handwritten by me and commented by claude, i am gonna bite you if you say this cmake is badly written
#
# Specify the minimum required version of CMake. The version range 3.14...3.31 means
# the project requires at least CMake 3.14 but will work with any version up to 3.31
cmake_minimum_required(VERSION 3.14...3.31)

# Define the project name as "Conan"
project(Conan)


# windows and arm doesnt work, dont try right now lmao.
# or instead of using this, you can select the proper kit from CMake Extension in vsc
set(TARGET_PLATFORM "native" CACHE STRING "Target platform (native, arm64-linux, amd64-windows)")
if(TARGET_PLATFORM STREQUAL "arm64-linux")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain-arm64-linux.cmake)
elseif(TARGET_PLATFORM STREQUAL "amd64-windows")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/toolchain-amd64-windows.cmake)
endif()

if(CMAKE_BUILD_TYPE EQUAL Debug)
    set(QT_DEBUG_PLUGINS 1)
    set(TF_CPP_MIN_LOG_LEVEL 0)
endif()
# Configure C++ standard settings
set(CMAKE_CXX_STANDARD 17)              # Use C++17 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # Require C++17 support (don't fall back to previous versions)
set(CMAKE_CXX_EXTENSIONS ON)            # Allow compiler-specific extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Generate compile_commands.json for tooling
# Configure Qt-specific build settings
set(CMAKE_AUTOMOC ON)    # Enable Qt's Meta-Object Compiler
set(CMAKE_AUTORCC ON)    # Enable automatic compilation of Qt's resource files
set(CMAKE_AUTOUIC ON)    # Enable automatic compilation of Qt's UI files

# Find required external packages 
set(QT_LIB_CMAKE_DIR /usr/local/Qt-6.8.1/lib/cmake)
set(CMAKE_PREFIX_PATH ${QT_LIB_CMAKE_DIR}/Qt6)
set(Qt6_DIR ${QT_LIB_CMAKE_DIR}/Qt6)
set(Qt6GuiTools_DIR ${QT_LIB_CMAKE_DIR}/Qt6GuiTools)
set(Qt6Widgets_DIR ${QT_LIB_CMAKE_DIR}/Qt6Widgets)
set(Qt6Core_DIR ${QT_LIB_CMAKE_DIR}/Qt6Core)
set(Qt6Svg_DIR ${QT_LIB_CMAKE_DIR}/Qt6Svg)


find_package(Crow REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg)  # Qt6 components
find_package(spdlog REQUIRED)    # Logging library
find_package(glslang REQUIRED)   # GLSL compiler
find_package(Protobuf REQUIRED)  # Protocol Buffers
find_package(PkgConfig REQUIRED) # Package configuration tool
find_package(fmt REQUIRED)       # String formatting library



# Use pkg-config to find additional libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHROMAPRINT REQUIRED libchromaprint)  # Audio fingerprinting library
pkg_check_modules(VDPAU QUIET vdpau)    # Video acceleration (optional)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)  # Explicitly get GTK3
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.1)  # WebKit with GTK3
pkg_check_modules(X11 QUIET IMPORTED_TARGET x11)


file(GLOB SOURCES 
${CMAKE_SOURCE_DIR}/src/main.cpp 
${CMAKE_SOURCE_DIR}/src/server.cpp 
${CMAKE_SOURCE_DIR}/src/include/assets/*.cpp 
${CMAKE_SOURCE_DIR}/src/layouts/*.cpp
${CMAKE_SOURCE_DIR}/src/workers/*.cpp
)

link_directories(${GTK_LIBRARY_DIRS})
link_directories(${WEBKIT_LIBRARY_DIRS})
link_directories(${X11_LIBRARY_DIRS})
# Create the executable target from the main source file
add_executable(${PROJECT_NAME} ${SOURCES})

# Add preprocessor definitions that will be available during compilation
# These are important POSIX and GNU specific macros that enable various features
target_compile_definitions(${PROJECT_NAME} PRIVATE
    # Enable POSIX.1b real-time extensions
    -D_POSIX_C_SOURCE=199309L
    # Enable GNU extensions
    -D_GNU_SOURCE
    # Enable macros for constant values in stdint.h
    -D__STDC_CONSTANT_MACROS
    # Enable printf format macros in inttypes.h
    -D__STDC_FORMAT_MACROS
    # Enable limit macros in stdint.h
    -D__STDC_LIMIT_MACROS
)


# Define vendor directory paths for better organization
set(FFMPEG_VENDOR_DIR       ${CMAKE_SOURCE_DIR}/src/vendor/ffmpeg)
set(GLSLANG_VENDOR_DIR      ${CMAKE_SOURCE_DIR}/src/vendor/glslang)
set(TENSORFLOW_VENDOR_DIR   ${CMAKE_SOURCE_DIR}/src/vendor/tensorflow)

# Configure include directories for the target
# PRIVATE means these includes are only used internally by this target
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/layouts
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/include/assets
    ${Crow_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}                    # X11 headers
    ${VDPAU_INCLUDE_DIRS}                 # VDPAU headers
    ${TAGLIB_INCLUDE_DIRS}
    ${WEBKIT_INCLUDE_DIRS}
    ${GTK3_INCLUDE_DIRS}
    /usr/local/include/essentia          # Audio analysis library
    samplerate                           # Audio sample rate conversion
    ${Protobuf_INCLUDE_DIRS}            # Protocol Buffers headers
    # GLSL and SPIR-V related headers
    ${GLSLANG_VENDOR_DIR}/SPIRV
    ${GLSLANG_VENDOR_DIR}/external/spirv-headers/include
    ${GLSLANG_VENDOR_DIR}/External/spirv-tools
    /usr/local/include/spirv-tools/
    # Eigen matrix library headers
    /usr/include/eigen3
    /usr/include/eigen3/unsupported
    # TensorFlow headers
    ${TENSORFLOW_VENDOR_DIR}/include
    ${CHROMAPRINT_INCLUDE_DIRS}          # Chromaprint headers
    ${YAML_INCLUDE_DIR}                  # YAML parser headers
    # FFmpeg related headers
    ${FFMPEG_VENDOR_DIR}
    ${FFMPEG_VENDOR_DIR}/libavformat
    ${FFMPEG_VENDOR_DIR}/libavformat/*.h
    ${FFMPEG_VENDOR_DIR}/libavcodec
    ${FFMPEG_VENDOR_DIR}/libavcodec/*.h
    ${FFMPEG_VENDOR_DIR}/libavdevice
    ${FFMPEG_VENDOR_DIR}/libavfilter
    ${FFMPEG_VENDOR_DIR}/libavresample
    ${FFMPEG_VENDOR_DIR}/libavutil
    ${FFMPEG_VENDOR_DIR}/libpostproc
    ${FFMPEG_VENDOR_DIR}/libswresample
    ${FFMPEG_VENDOR_DIR}/libswscale
    ${LLVM_INCLUDE_DIRS}
)


# Define paths to FFmpeg static libraries
set(LIBAVUTIL_LIBRARY       ${FFMPEG_VENDOR_DIR}/libavutil/libavutil.a)
set(LIBAVCODEC_LIBRARY      ${FFMPEG_VENDOR_DIR}/libavcodec/libavcodec.a)
set(LIBAVFORMAT_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavformat/libavformat.a)
set(LIBAVFILTER_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavfilter/libavfilter.a)
set(LIBSWRESAMPLE_LIBRARY   ${FFMPEG_VENDOR_DIR}/libswresample/libswresample.a)
set(LIBSWSCALE_LIBRARY      ${FFMPEG_VENDOR_DIR}/libswscale/libswscale.a)
set(LIBAVDEVICE_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavdevice/libavdevice.a)

# ======================= LINKER FLAGS AND SYMBOL MANAGEMENT =======================
#
# These flags control how symbols are handled during linking and at runtime.
#
# --exclude-libs,ALL
# This flag marks all symbols from static libraries as hidden, preventing symbol 
# conflicts between different static libraries that might define the same symbols.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--exclude-libs,ALL -Wl,--allow-multiple-definition")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL -Wl,--allow-multiple-definition")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
file(WRITE ${CMAKE_BINARY_DIR}/symbol_control.version 
"LLVM_WEBKIT {
    global:
        webkit_*;
        gtk_*;
        gdk_*;
        g_*;
        WebKit*;
        GTK*;
    local: 
        _LLVM*;
        llvm*;
        LLVM*;
        *;
};

TENSORFLOW {
    global:
        tensorflow_*;
        tf_*;
        TF_*;
    local: *;
};"
)

# ========================= LIBRARY LINKING ORDER =================================
#
# The order of library linking is crucial to resolve symbols correctly.
# We use --push-state and --pop-state with --as-needed/--no-as-needed to create
# isolated groups of libraries with different linking behaviors.
#
# Group 1: GTK and WebKit (--no-as-needed)
# Forces inclusion of all symbols from these libraries even if they appear unused.
# This prevents symbol resolution issues with GUI components.
#
# Group 2: Regular libraries
# Linked normally, allowing the linker to exclude unused symbols.
#
# Group 3: TensorFlow (--as-needed)
# Only includes actually used symbols, preventing conflicts with other libraries
# that might use similar dependencies (like LLVM).
target_link_libraries(${PROJECT_NAME} PRIVATE
    # First load GTK and WebKit with their symbols protected
    -Wl,--push-state
    -Wl,--no-as-needed
    -Wl,--whole-archive
    PkgConfig::GTK3
    PkgConfig::WEBKIT
    -Wl,--no-whole-archive
    -Wl,--pop-state

    Qt6::Core
    Qt6::Widgets
    Qt6::Svg

    # X11 and video-related libraries
    PkgConfig::X11
    ${X11_LIBRARIES}
    ${VDPAU_LIBRARIES}
    va
    va-drm
    glslang

    # Audio processing libraries
    /usr/local/lib/libessentia.a
    # FFmpeg libraries must be linked in this specific order due to dependencies
    ${LIBAVDEVICE_LIBRARY}
    ${LIBAVFILTER_LIBRARY}
    ${LIBAVFORMAT_LIBRARY}
    ${LIBAVCODEC_LIBRARY}
    ${LIBSWRESAMPLE_LIBRARY}
    ${LIBSWSCALE_LIBRARY}
    ${LIBAVUTIL_LIBRARY}
    
    samplerate
    ${CHROMAPRINT_LIBRARIES}
    ${TAGLIB_LIBRARIES}
    tag
    fftw3
    fftw3f

    # Utility libraries
    ${Crow_LIBRARIES}
    yaml
    z
    bz2
    lzma
    spdlog::spdlog
    fmt
    ${Protobuf_LIBRARIES}

    # System libraries
    dl
    pthread
    m
    rt

    # TensorFlow last with symbol isolation
    -Wl,--push-state
    -Wl,--as-needed
    -Wl,--no-undefined
    -Wl,--version-script=${CMAKE_BINARY_DIR}/symbol_control.version
    ${TENSORFLOW_VENDOR_DIR}/lib/libtensorflow.so.2
    ${TENSORFLOW_VENDOR_DIR}/lib/libtensorflow_framework.so.2
    -Wl,--pop-state
)

# ========================= RPATH CONFIGURATION ==================================
#
# RPATH (Runtime Path) tells the dynamic linker where to look for shared libraries
# when the program runs. This is crucial for finding the correct versions of
# libraries, especially when using custom-built versions.
#
# BUILD_WITH_INSTALL_RPATH:
#   TRUE means use the INSTALL_RPATH instead of the build directory paths
#
# INSTALL_RPATH:
#   - $ORIGIN means "look in the same directory as the executable"
#   - Additional paths are searched in order
#   This helps find:
#   1. Libraries in the same directory as the executable
#   2. Custom TensorFlow libraries
#   3. Custom-built Qt libraries
#
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN:${TENSORFLOW_VENDOR_DIR}/lib:/usr/local/Qt-6.8.1/lib"
    INSTALL_RPATH_USE_LINK_PATH TRUE
)
target_compile_definitions(${PROJECT_NAME} PRIVATE ${LLVM_DEFINITIONS} QT_NO_KEYWORDS)
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⣀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⡴⠾⠛⠋⠉⠉⠀⠀⠀⠀⠀⠈⠉⠉⠛⠛⠶⢦⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⡴⠾⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠙⠲⢦⣄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡴⠞⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠙⠳⢦⣄⠀⠀⠀⠀⢀⣠⠴⠖⠛⠛⠛⠶⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⣀⠀⠀⢀⣤⠞⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠳⢦⣠⠞⠋⠀⠀⠀⠀⠀⠀⠀⠈⢳⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⡶⠛⠉⠉⠀⠀⠀⠀⠀⢉⣵⠋⠁⠀⠀⠀⢀⣠⣶⣶⣶⣤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⣿⣿⣿⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣆⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡶⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠁⠀⠀⠀⢀⣰⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣿⣿⣿⣿⣿⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⡆⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⠞⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢈⣿⣿⣿⣿⣿⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣄⠀⠀⠀⠀⠈⠻⢿⣿⣿⡿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣷⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡞⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠹⠿⠿⠟⠉⠀⠀⠀⢀⣤⣿⣶⣀⠀⣠⣾⣿⣷⣦⣀⠀⣠⣄⣄⣀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡆⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣶⣆⣠⣾⣿⡿⠻⢿⣿⣿⣿⠟⠁⠙⠻⣿⣿⣿⣿⣿⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠛⣷⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⣿⣿⡏⠀⠀⠀⠀⠀⣀⣤⠶⣿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⢿⣿⣿⡿⠛⣁⣀⣠⣤⣽⣏⣀⡀⠀⠀⠀⠀⠉⠛⢿⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⡆⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⢀⣾⣿⣿⣿⣿⣿⠀⠀⠀⢀⣰⡾⠏⠁⣸⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⢿⣱⣶⣿⣿⣉⣷⣶⣶⣾⣉⣹⠷⣆⣀⠀⠀⠀⠀⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣇⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⣠⣾⣿⣿⣿⣿⣿⣿⠀⣀⡴⠛⠁⠀⠀⣼⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣾⣿⠿⠛⢋⡉⠉⠀⠀⡀⠉⠉⠙⠻⠶⣭⣛⡷⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⡆⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⡿⠛⠁⠀⠀⠀⢀⡾⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣾⣿⡿⢋⠀⡀⠄⣸⡏⠀⠀⠁⡄⠂⢀⠂⡀⠑⣶⣿⡙⠳⢮⣟⠷⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⡄⠀⠀⠀⠀⠀⠀⠀⠀⢿⡇⠀⠀⠀⠀
# ⠀⠀⠀⣠⣿⣿⣿⣿⣿⣿⣿⠟⠁⠀⠀⠀⠀⠀⢀⡾⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣾⣿⠟⠉⢸⡿⢀⠄⡀⠀⣼⡇⠀⠀⠀⣿⡄⠀⡀⠄⢠⣿⣿⡇⠀⠀⠈⢻⡶⣯⣛⠶⣤⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⡄⠀⠀⠀⠀⠀⠀⠀⠐⣷⠀⠀⠀⠀
# ⠀⢀⣼⣿⣿⣿⣿⣿⡿⠋⠁⠀⠀⠀⠀⠀⠀⠀⣼⠃⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣾⡿⠛⢁⠀⠂⣿⠇⠀⠐⠀⠀⣿⠁⠀⠀⠀⣿⠆⢀⠠⠀⢢⣿⣿⠀⠀⠀⠀⠸⣷⢀⡉⠻⢾⣽⡳⢤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⡀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀
# ⢠⣿⣿⣿⣿⣿⠟⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⢰⡟⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⡿⡿⠄⡁⠂⠠⢩⣿⡴⠀⠁⠀⢡⣿⠀⠀⠀⠀⣿⠂⠀⢀⠌⣸⣿⡟⠀⠀⠀⠀⠀⢹⣿⣇⠀⠁⢸⣿⢶⣍⡷⢦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣧⠀⠀⢀⣤⣤⣤⣶⣿⠀⠀⠀⠀
# ⣾⣿⣿⡿⠋⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⠁⠀⠀⠀⠀⠀⢀⣴⣿⣿⡋⠀⠁⠀⠀⠈⡀⣾⣏⠀⠀⠀⠀⣾⡏⠀⠀⠀⢻⣿⡇⡰⠌⠀⣼⣿⡇⠠⠶⠞⠛⠓⠾⣿⣿⡀⠀⠀⣿⡄⠈⠙⢷⣯⡻⢦⣄⠀⠀⠀⠀⠀⠀⠀⠹⣷⣿⣿⣿⣿⣿⣿⣿⡀⠀⠀⠀
# ⠈⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⠃⠀⠀⠀⠀⢀⣰⣿⠟⠁⣿⠁⠀⠀⠄⢀⠀⣰⣿⠃⡀⢀⠠⢱⣿⠀⠀⠀⠀⣾⣏⠐⠃⠀⣀⣿⡿⠀⠀⠀⠀⠀⠀⠀⢸⣿⣷⡀⠀⠻⣷⡀⠁⢺⡏⠙⢶⣍⠷⣄⠀⠀⠀⠀⠀⠀⠹⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣼⠃⠀⠀⠀⠀⣠⣾⠟⠁⠀⠀⣿⠀⠀⠈⠔⠀⡰⣼⣿⠁⡀⠄⢠⣾⠇⢰⡇⠀⢠⣿⢄⡯⠀⠠⣽⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠻⣿⣷⣄⠁⡹⣧⢂⢸⣧⠀⢼⡟⠳⣿⣻⣤⡀⠀⠀⠀⠀⠹⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡾⠃⠀⠀⠀⢠⣾⡿⠁⠀⠠⠀⠀⣿⠀⠀⠁⠀⢒⣽⢿⣿⡆⠀⢄⣹⡟⣡⡿⠀⠀⣾⣷⡿⠃⢀⣵⣟⡿⠀⠀⠀⠀⠀⢀⣀⡀⠀⠀⠹⣧⠻⣧⡐⢹⣧⠘⣿⠀⢸⣿⠀⠐⣟⢿⣷⣄⡀⠀⠀⠀⢻⣿⣿⣿⣿⣿⡇⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡾⠃⠀⠀⢀⣴⣿⡏⠀⡀⢠⡆⠀⠀⣿⠀⠀⠐⣁⣾⢯⣿⡟⠀⢄⣾⣿⣼⡟⠀⢀⣾⣿⡿⠁⣐⣾⣿⣿⠃⠀⠀⢀⡶⠛⠋⠉⠀⠀⠀⠀⠙⢷⡌⠻⣦⣿⣯⣿⠀⠘⣿⡈⠀⢸⡇⠙⢿⣷⡀⠀⠀⠀⢻⣿⣿⣿⣿⡇⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⠃⠀⠀⣰⡿⠋⢸⡏⠀⠀⣸⡇⠀⠀⣿⠄⠀⣘⣿⢿⣿⣿⠡⣰⣿⣿⣿⡟⠀⣴⣿⣿⣿⠃⣰⠟⣹⠿⠁⠀⠀⠀⠀⢳⡶⣶⣛⣚⣟⣲⠶⣦⣄⠙⠶⣬⡙⠿⣿⡠⠈⢹⡇⠀⠘⣷⠀⠀⠻⣿⣦⡀⠀⠀⠹⣿⣿⠟⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⠀⢠⣾⠟⠁⠀⢸⠇⢀⠀⣿⠁⠀⠀⣿⡀⣴⣿⣥⣿⣿⣿⣾⣿⣿⣿⣿⣶⡿⢿⣿⣿⣥⠾⠻⠞⠉⠀⠀⠀⠀⠀⠀⢸⣿⢿⣿⣿⣿⣿⣿⣶⣭⣄⠀⠀⠀⠀⣿⣷⠀⠸⣷⡀⠀⣿⡄⠀⠀⠈⠻⣷⣄⠀⠀⠈⠁⠀⣦⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣥⣿⠃⠀⠀⠀⢼⡃⠀⡂⣿⠀⠀⠀⣿⣿⣿⣹⣿⣿⠿⠛⣽⣿⣿⣿⣿⣿⡇⠀⠉⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⣿⣿⡿⢿⣿⣿⢿⣿⣄⠀⠀⢻⡟⠀⢂⣿⣷⣄⢸⣇⠀⠀⠈⠀⠙⣿⣧⡀⠀⠀⠀⢻⡄⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⡇⠀⠀⠀⠀⣿⠄⠀⡅⣿⡆⠀⠀⣿⡅⢩⣿⡿⠁⠀⢠⣿⣿⣿⣿⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣿⣿⣿⣿⣷⣾⣿⣿⠀⠹⣿⡇⠀⣿⡃⢈⠰⣸⣧⣍⢻⣿⠀⠀⠀⠀⠀⠈⢻⣷⡄⠀⠀⠀⢷⡀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⡇⠀⠀⠀⢀⣿⢰⣄⣲⣿⣷⢀⠀⢸⡇⠈⣿⠃⠀⠀⢸⣿⣿⣻⣽⣻⣿⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠯⢻⣯⣭⡄⢻⣿⣿⣿⠀⢀⡿⠀⠀⣽⡇⠤⢦⠛⡷⠈⢿⣿⠄⠀⠀⠀⠰⠀⢄⡛⢿⣆⠀⠀⠈⣷⡀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⢿⡄⠀⠀⠀⢸⣇⠀⡀⣹⣿⣿⣿⣦⣸⣧⠀⠙⠇⠀⠀⠘⠉⣉⠁⠈⠉⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢲⣿⣿⣴⣦⣿⣿⡿⠧⠆⠀⠀⠀⠀⣻⡆⠀⠘⢻⣿⣶⠞⢿⣷⠀⠀⠀⠀⢿⣦⢻⡌⢻⣆⠀⢀⣿⠇
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣇⠀⠀⠀⣿⡟⠈⢀⠸⣿⡏⢿⣿⣿⣧⡀⠀⠀⠀⠀⠀⠻⣿⡿⠾⠟⠛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠉⠛⠉⠀⠀⠀⠀⠀⠀⠀⣿⡗⠀⠡⠘⣿⣎⡗⣾⡇⠀⠀⠀⠀⠀⢻⣟⣷⠈⣿⡶⠛⠁⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⣄⠀⢠⣿⠀⠌⡀⠐⣿⣧⠀⢻⣿⣿⣿⣶⣶⣤⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣠⣄⣀⠀⠀⠀⠀⣿⠀⠌⡀⢀⠻⣿⠰⢻⣧⠀⠀⠀⡶⠀⠈⣿⣽⡟⠁⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠹⣦⣼⡇⠌⡐⠀⠀⢿⣿⠀⠘⣿⣿⣿⣿⣿⣿⣿⣧⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⡴⠟⣿⣿⣿⣿⣿⣿⣷⣦⣤⣿⠀⠌⠐⠀⢒⣿⡇⠘⣿⠀⠀⠀⣿⠀⠀⢿⣷⣿⡀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⡐⠰⠀⠀⠸⢿⡂⠀⢛⣻⢿⣿⣿⣿⡿⠉⠳⣦⡀⠀⠀⠀⠀⠀⢀⣠⣤⠶⠖⠛⠛⠛⠒⠶⢦⣄⣀⣤⠞⠋⠁⠀⠀⠻⣿⣿⠿⠿⠿⠿⠿⣿⣿⠀⠂⠀⢨⣏⣼⣧⠐⣿⠀⠀⠠⣿⠀⠀⠚⣿⢹⣷⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⡿⣁⠀⢀⣀⠀⠀⢻⡇⠀⠀⠘⢻⣿⣿⣿⡀⠀⠀⠈⠙⠳⢤⣤⡤⠞⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠐⠚⠉⠀⠀⠀⠀⠀⢀⡾⠃⠀⠀⠀⢀⣴⣿⣿⡏⠀⠀⠀⢸⣗⠀⣿⠀⣿⡆⠀⠀⣿⠆⠀⣀⢿⡆⢿⡄⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣾⣿⣿⢸⡟⠙⣷⠀⠸⣿⠀⢀⣀⠈⣿⡿⣮⡳⣄⠀⠀⠀⠀⠀⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⡀⠀⠀⠀⠀⣠⡟⠁⠀⠀⢀⡴⢿⣿⣿⣿⣁⡀⠀⠀⢸⣿⡀⣿⡄⣿⡇⠀⠀⣿⡇⡀⢻⣾⡇⢸⣷⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⡇⣿⢸⡇⠀⣽⠀⢤⣿⣼⠟⢹⡇⠘⣷⣾⡟⢿⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣶⣄⡤⠀⠀⢸⣿⠀⠀⣠⡾⠋⠀⣾⣿⣿⡿⣻⠄⠀⠀⢸⣿⡇⣹⣧⣿⠁⠀⠀⣿⡃⣿⢸⡗⣷⠀⣿⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣿⠁⣿⢸⡇⠀⢾⠈⢽⣿⣿⠀⣸⡇⠀⠸⣿⣿⡀⣌⢻⡦⠀⠀⠀⢀⣴⣶⣦⣤⠄⠀⠀⠀⠀⢦⣿⣿⣹⣿⡧⠂⠀⠀⠙⣿⣿⠁⠀⠀⣰⣿⣿⣿⢱⣿⠀⠀⠀⣾⣿⡇⢿⣿⠏⣼⡇⠀⣿⠄⣿⣿⠇⣿⠀⣿⡄⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡿⠀⣿⣸⡇⠀⣾⠀⣸⣿⠇⢀⣿⠀⠀⠰⣿⣿⣿⣸⣿⠁⠀⠀⢀⣿⣿⣹⣿⣿⠁⠀⠀⠀⠀⠸⣿⣿⣿⣿⡿⠀⠀⠀⠀⠈⣿⡆⠀⣀⣿⡿⢿⣿⣿⡏⠀⠀⢀⣿⣿⣷⣾⡏⣰⡿⠀⠀⣿⠀⢿⣿⣆⡷⣸⣿⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇⠀⢻⣿⡇⠀⢸⣇⣌⡿⠀⣼⣿⡀⠀⠀⠈⢻⣾⣿⣿⠀⠀⠀⠈⣿⣿⣿⣿⠗⠀⠀⠀⠀⠀⠀⠹⠿⠿⠟⠁⠀⠀⠀⠀⠀⠸⣿⡿⠛⠁⢀⣼⣿⣿⣡⠀⠀⣸⡟⣼⣧⣿⣷⠿⠁⠀⣸⣿⡴⣼⡏⣸⣷⡿⠃⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠸⣿⠀⠘⣿⡇⠀⢸⣿⣿⠃⠀⣿⣯⢿⣄⠀⠀⠀⢹⣶⡇⠀⠀⠀⠀⠈⠛⠋⠉⠀⠀⠀⠀⠀⠀⢀⣀⣤⣶⡷⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⢀⣾⠟⣾⣻⡇⠀⣠⣿⠃⣽⣿⣿⡏⢀⣴⢠⣿⣿⣿⣟⣤⡿⠋⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠹⣷⣤⣽⣧⠀⠘⣿⡏⠀⢀⣿⣿⡿⣿⣷⣄⠀⢠⣿⡇⠀⠀⠀⠀⠀⠀⠀⣤⣤⣤⣾⣷⣶⣾⠟⠿⣿⠋⠀⠀⠀⠀⠀⠀⠀⢀⡿⢀⣴⠟⢁⣾⣿⣿⣿⠃⣼⣿⣰⡿⣫⣿⣵⣟⣵⣿⣿⣿⣷⠞⠉⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡼⠛⠙⣷⡀⠀⠙⠃⠀⠀⠿⣮⣿⣶⣭⣟⣿⠷⢾⣷⠀⠀⠀⠁⠀⠀⠀⠙⢿⡟⠉⠀⠉⠁⠀⠘⠀⠀⠀⠀⠀⠀⠀⠀⢠⡾⠟⠋⢁⣠⠾⣫⣾⣿⣥⣾⣿⡿⠿⠚⠛⠋⠉⠉⠉⢹⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡾⠛⠳⣄⠀⠘⣷⣀⣀⣀⡀⠀⡆⠀⠈⠉⠀⠈⠉⠁⠀⠹⣧⠀⠀⠀⠀⠀⠀⠀⠉⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⡼⠋⠀⠀⠐⠛⠛⠛⠉⠉⠈⣠⡾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠉⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡏⢰⡄⠀⠘⣇⠀⢸⣯⡀⠉⠉⠻⣶⡀⠀⠀⠀⢰⡇⠀⠀⠀⠈⠳⢦⣤⣄⣀⣀⢀⣤⠶⢶⣄⠀⠀⠀⠀⠀⠀⠀⠠⣾⡛⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⡾⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⣿⠀⠀⠙⣆⠀⣽⣦⣾⠋⠓⣶⠄⠀⠈⢿⡀⠀⠀⣾⠁⠀⠀⠀⠀⠀⠀⠀⠈⣩⡿⠋⠀⠀⠀⠈⢷⣄⡀⠀⠀⠀⠀⠀⠾⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣾⢿⡀⠀⠀⠙⠓⠛⠋⠉⣠⠟⠁⠀⠀⠀⢸⡇⠀⣰⠟⠀⠀⠀⠀⠀⠀⠀⣠⠞⠃⠀⠀⠀⠀⠀⠀⠰⠛⠙⢦⡀⠀⠀⠀⠀⢿⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⠘⣧⡀⠀⠀⠀⠀⠀⠠⠏⠀⠀⠀⠀⢀⡾⠁⣰⡟⠀⠀⠀⠀⠀⠀⣰⣼⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣤⠀⠀⠀⠈⢷⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣸⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣧⣼⠋⠙⢦⣄⡀⠀⠀⠀⠀⠀⢀⣠⡴⢿⡿⢛⡿⠀⠀⠀⠀⠀⠀⢀⣿⡁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⢦⣄⡀⠘⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡀⠀⢈⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⠚⢻⣻⣿⠀⠀⠀⠈⠉⠙⠛⠛⠛⠋⠉⢿⣤⣿⠀⣸⠃⠀⠀⠀⠀⠀⠀⠀⠈⢷⣄⣠⡾⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣤⡸⣇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡠⠖⠁⠀⠀⣼⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
# ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿⡄⠀⠀⠀⢀⠀⠀⠀⠀⡀⢀⣾⠉⣃⣠⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢹⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⣧⣭⢻⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⣠⣤⠖⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀