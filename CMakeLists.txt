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

# Configure C++ standard settings
set(CMAKE_CXX_STANDARD 17)              # Use C++17 standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)     # Require C++17 support (don't fall back to previous versions)
set(CMAKE_CXX_EXTENSIONS ON)            # Allow compiler-specific extensions
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Generate compile_commands.json for tooling
set(GALLIUM_DRIVER d3d12)               # https://askubuntu.com/questions/1530856/wsl2-mesa-error-zink-failed-to-choose-pdev-after-glxinfo-b
set(QT_QPA_PLATFORM offscreen)          # https://github.com/NVlabs/instant-ngp/discussions/300

# Configure Qt-specific build settings
set(CMAKE_AUTOMOC ON)    # Enable Qt's Meta-Object Compiler
set(CMAKE_AUTORCC ON)    # Enable automatic compilation of Qt's resource files
set(CMAKE_AUTOUIC ON)    # Enable automatic compilation of Qt's UI files

# Find required external packages 
find_package(Crow REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test WebEngineWidgets WebEngineCore Svg)  # Qt6 components
find_package(spdlog REQUIRED)    # Logging library
find_package(glslang REQUIRED)   # GLSL compiler
find_package(Protobuf REQUIRED)  # Protocol Buffers
find_package(PkgConfig REQUIRED) # Package configuration tool
find_package(fmt REQUIRED)       # String formatting library
find_package(X11 QUIET)         # X11 window system (optional)

# Use pkg-config to find additional libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(CHROMAPRINT REQUIRED libchromaprint)  # Audio fingerprinting library
pkg_check_modules(VDPAU QUIET vdpau)    # Video acceleration (optional)
pkg_check_modules(TAGLIB REQUIRED taglib)

file(GLOB SOURCES 
${CMAKE_SOURCE_DIR}/src/main.cpp 
${CMAKE_SOURCE_DIR}/src/server.cpp 
${CMAKE_SOURCE_DIR}/src/include/assets/*.cpp 
${CMAKE_SOURCE_DIR}/src/layouts/*.cpp
${CMAKE_SOURCE_DIR}/src/workers/*.cpp
)
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
set(FFMPEG_VENDOR_DIR   ${CMAKE_SOURCE_DIR}/src/vendor/ffmpeg)
set(GLSLANG_VENDOR_DIR  ${CMAKE_SOURCE_DIR}/src/vendor/glslang)

# Configure include directories for the target
# PRIVATE means these includes are only used internally by this target
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/layouts
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/include/assets
    ${Crow_INCLUDE_DIR}
    ${X11_INCLUDE_DIRS}                    # X11 headers
    ${VDPAU_INCLUDE_DIRS}                 # VDPAU headers
    ${TAGLIB_INCLUDE_DIRS}
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
    /usr/local/tensorflow/include
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
)


# Define paths to FFmpeg static libraries
set(LIBAVUTIL_LIBRARY       ${FFMPEG_VENDOR_DIR}/libavutil/libavutil.a)
set(LIBAVCODEC_LIBRARY      ${FFMPEG_VENDOR_DIR}/libavcodec/libavcodec.a)
set(LIBAVFORMAT_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavformat/libavformat.a)
set(LIBAVFILTER_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavfilter/libavfilter.a)
set(LIBSWRESAMPLE_LIBRARY   ${FFMPEG_VENDOR_DIR}/libswresample/libswresample.a)
set(LIBSWSCALE_LIBRARY      ${FFMPEG_VENDOR_DIR}/libswscale/libswscale.a)
set(LIBAVDEVICE_LIBRARY     ${FFMPEG_VENDOR_DIR}/libavdevice/libavdevice.a)
set(TF_LIB_DIR              "/usr/local/lib")

# Link all required libraries to the target
# WARNING: FFmpeg libraries must be linked in this specific order due to dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${TF_LIB_DIR}/libtensorflow.so.2
    # ${TF_LIB_DIR}/libtensorflow_framework.so.2

    dl
    pthread
    m
    rt

    ${Crow_LIBRARIES}
    /usr/local/lib/libessentia.a        # Audio analysis library
    samplerate                          # Sample rate conversion
    # FFmpeg libraries (order matters!)
    ${LIBAVDEVICE_LIBRARY}
    ${LIBAVFILTER_LIBRARY}
    ${LIBAVFORMAT_LIBRARY}
    ${LIBAVCODEC_LIBRARY}
    ${LIBSWRESAMPLE_LIBRARY}
    ${LIBSWSCALE_LIBRARY}
    ${LIBAVUTIL_LIBRARY} 
    # Video acceleration libraries
    va
    va-drm
    ${X11_LIBRARIES}
    ${VDPAU_LIBRARIES}
    # Various other dependencies
    glslang
    ${Protobuf_LIBRARIES}
    tag                                 # Audio metadata library
    fftw3                              # Fast Fourier Transform
    fftw3f
    yaml                               # YAML parser
    z                                  # Compression
    bz2                                # Compression
    lzma                               # Compression
    # Qt libraries
    Qt6::Core
    Qt6::Widgets
    Qt6::Test
    Qt6::WebEngineWidgets
    Qt6::WebEngineCore
    Qt6::Svg
    # Logging and formatting
    spdlog::spdlog
    fmt
    ${CHROMAPRINT_LIBRARIES}           # Audio fingerprinting
    ${TAGLIB_LIBRARIES}
)

add_compile_definitions(QT_NO_KEYWORDS)