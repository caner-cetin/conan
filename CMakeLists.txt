cmake_minimum_required(VERSION 3.14)
project(Conan)

# Basic C++ settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Platform-specific settings
if(APPLE)
    execute_process(
        COMMAND brew --prefix llvm
        OUTPUT_VARIABLE LLVM_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -arch arm64")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -frtti")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${LLVM_PREFIX}/lib -L${LLVM_PREFIX}/lib/c++ -Wl,-rpath,${LLVM_PREFIX}/lib")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi -Wl,-export_dynamic")
    link_libraries(
        "-L${LLVM_PREFIX}/lib"
        "-L${LLVM_PREFIX}/lib/c++"
        c++
        c++abi
    )

    add_compile_definitions(
        _LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS
        _LIBCPP_DISABLE_AVAILABILITY
    )
endif()

# Qt settings
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(QT_LIB_CMAKE_DIR /usr/local/Qt-6.8.1/lib/cmake)
set(CMAKE_PREFIX_PATH ${QT_LIB_CMAKE_DIR}/Qt6)
set(Qt6_DIR ${QT_LIB_CMAKE_DIR}/Qt6)

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent)
find_package(spdlog REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

set(ENV{PKG_CONFIG_PATH} "/opt/local/lib/pkgconfig/")

# Platform-specific library handling
if(APPLE)
    # Homebrew paths for macOS
    set(BREW_PATH /opt/homebrew)

    # Helper function to find libraries in Homebrew
    function(find_brew_library name var_name)
        find_library(${var_name}
            NAMES ${name} lib${name}
            PATHS ${BREW_PATH}/lib
            NO_DEFAULT_PATH
        )

        if(NOT ${var_name})
            message(FATAL_ERROR "${name} not found. Please install with: brew install ${name}")
        endif()
    endfunction()

    # Find required libraries
    find_brew_library(samplerate SAMPLERATE_LIBRARIES)
    find_brew_library(chromaprint CHROMAPRINT_LIBRARIES)
    find_brew_library(yaml YAML_LIBRARIES)

    # FFmpeg libraries
    find_brew_library(avcodec AVCODEC_LIBRARIES)
    find_brew_library(avformat AVFORMAT_LIBRARIES)
    find_brew_library(avutil AVUTIL_LIBRARIES)
    find_brew_library(swresample SWRESAMPLE_LIBRARIES)
    find_brew_library(tag TAGLIB_LIBRARIES)
    find_brew_library(fftw3 FFTW3_LIBRARIES)
    find_brew_library(fftw3f FFTW3F_LIBRARIES)
    find_brew_library(tensorflow TENSORFLOW_LIBRARIES)

    # Find include directories
    find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h PATHS ${BREW_PATH}/include)
else()
    # Linux package handling
    pkg_check_modules(SAMPLERATE REQUIRED samplerate)
    pkg_check_modules(CHROMAPRINT REQUIRED chromaprint)
    pkg_check_modules(YAML REQUIRED yaml)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
    pkg_check_modules(TENSORFLOW REQUIRED tensorflow)

    set(FFTW3_LIBRARIES fftw3)
    set(FFTW3F_LIBRARIES fftw3f)
endif()

pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(WEBKIT REQUIRED IMPORTED_TARGET webkit2gtk-4.0)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)

# Source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/include/assets/**/*.cpp
    ${CMAKE_SOURCE_DIR}/src/layouts/*.cpp
    ${CMAKE_SOURCE_DIR}/src/workers/*.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/layouts
    ${CMAKE_SOURCE_DIR}/src/workers
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/include/assets
    ${CMAKE_SOURCE_DIR}/src/vendor/crow/include
    ${CURL_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIR}
    /usr/local/include/essentia
    /opt/homebrew/Cellar/eigen/3.4.0_1/include/eigen3
    ${SQLite3_INCLUDE_DIRS}
    ${WEBKIT_INCLUDE_DIRS}
    ${TAGLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE

    # Qt
    Qt6::Core
    Qt6::Widgets
    Qt6::Svg
    Qt6::Concurrent

    # Media libraries
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    /usr/local/lib/libessentia.a
    PkgConfig::GTK3
    PkgConfig::WEBKIT
    ${SAMPLERATE_LIBRARIES}
    ${CHROMAPRINT_LIBRARIES}
    PkgConfig::TAGLIB
    ${FFTW3_LIBRARIES}
    ${FFTW3F_LIBRARIES}

    ${CURL_LIBRARIES}
    ${YAML_LIBRARIES}
    spdlog::spdlog
    fmt::fmt
    ${SQLite3_LIBRARIES}
    ${TENSORFLOW_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_NO_KEYWORDS)

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