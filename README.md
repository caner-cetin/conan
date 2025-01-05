## conan
smol music recommendation engine based on local library folders.

![alt text](./static/ui.png)

*ui is not final*

- [conan](#conan)
- [features](#features)
- [download](#download)
- [compile yourself](#compile-yourself)
  - [libs](#libs)
  - [ffmpeg](#ffmpeg)
  - [essentia](#essentia)
  - [glslang](#glslang)
  - [gtest](#gtest)
  - [tensorflow](#tensorflow)
  - [crow](#crow)
  - [things may help you in development](#things-may-help-you-in-development)
    - [generate binary headers for embedding assets](#generate-binary-headers-for-embedding-assets)
    - [vscode setup](#vscode-setup)
- [why](#why)
  - [why do you have `time.h` at include folder](#why-do-you-have-timeh-at-include-folder)
  - [why did you create this project](#why-did-you-create-this-project)
- [problems, todos, and many more rants](#problems-todos-and-many-more-rants)
  - [cannot link tensorflow and tensorflow framework](#cannot-link-tensorflow-and-tensorflow-framework)
  - [windows and arm toolchains (mingw, etc) does not work.](#windows-and-arm-toolchains-mingw-etc-does-not-work)

## features
..
## download
...
## compile yourself
trust me, this is not worth your time. if you follow these steps, and for uncertain reasons you decide to compile this application, you are my best friend from now on.

### libs
```bash
sudo add-apt-repository ppa:oibaf/graphics-drivers
sudo apt-get update
sudo apt-get install \
    libva-dev \
    libyaml-dev \
    libvdpau-dev \
    libx11-dev \
    libsamplerate0-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libeigen3-dev \
    libfftw3-dev \
    libchromaprint-dev \
    libspdlog-dev \
    libfmt-dev \
    libtagc0-dev \
    libva-drm2 \
    libbz2-dev \
    liblzma-dev \
    uuid-dev \
    libcap-dev \
    libzmq3-dev \
    libpwquality-dev \
    libmemcached-dev \
    libjemalloc-dev
    uwsgi \
    zlib1g-dev \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-webengine-dev \
    qt6-svg-dev
```
### ffmpeg
this is gonna override your default ffmpeg installation, but you can bump to latest version after compiling essentia with your package manager like `apt-get install ffmpeg`. you dont need the compiled binaries, we only need libraries, and we include them from `vendor/ffmpeg` folder. after `sudo make install` and compiling essentia, you can just do `apt-get install ffmpeg` then override everything.

if you dont want to install 4.4, pick a version [lower than 5](https://github.com/MTG/essentia/issues/1248), and pick a version that released before [September 15, 2021](https://patchwork.ffmpeg.org/project/ffmpeg/patch/AM7PR03MB6660E1F8A57B76DF6578148B8FDB9@AM7PR03MB6660.eurprd03.prod.outlook.com/) [from here](https://ffmpeg.org/releases)
```bash
mkdir -p src/vendor
wget https://ffmpeg.org/releases/ffmpeg-4.4.tar.xz -O src/vendor/ffmpeg-4.4.tar.xz
mkdir -p src/vendor/ffmpeg && tar -xf src/vendor/ffmpeg-4.4.tar.xz -C src/vendor/ffmpeg --strip-components=1 && rm src/vendor/ffmpeg-4.4.tar.xz
cp misc/ffmpeg.patch src/vendor/ffmpeg/ffmpeg.patch
cd src/vendor/ffmpeg
patch -i ffmpeg.patch
./configure --disable-doc \
--disable-htmlpages \
--disable-manpages \
--disable-podpages \
--disable-txtpages \
--pkg-config-flags="--static" \
--ld="g++"
sudo make
sudo make install
sudo rm libavdevice/decklink*
```
### essentia
```bash
git clone https://github.com/MTG/essentia.git
cd essentia
python3 waf configure --build-static --with-tensorflow
python3 waf
sudo python3 waf install
```
### glslang
```bash
git clone https://github.com/KhronosGroup/glslang.git
cd glslang
python3 update_glslang_sources.py
BUILD_DIR="build"
cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$(pwd)/install"
make -j4 install
sudo ln -s /usr/local/bin/glslang /usr/bin/glslang
sudo ln -s /usr/local/bin/spirv-remap /usr/bin/spirv-remap
```
### gtest
```bash
sudo apt-get install -y libgtest-dev cmake
mkdir -p $HOME/build
cd $HOME/build
sudo cmake /usr/src/googletest/googletest
sudo make
sudo cp lib/libgtest* /usr/lib/
cd ..
sudo rm -rf build
sudo mkdir /usr/local/lib/googletest
sudo ln -s /usr/lib/libgtest.a /usr/local/lib/googletest/libgtest.a
sudo ln -s /usr/lib/libgtest_main.a /usr/local/lib/googletest/libgtest_main.a
```
### tensorflow

```bash
# get appropiate filename here https://www.tensorflow.org/install/lang_c#download_and_extract
FILENAME=libtensorflow-gpu-linux-x86_64.tar.gz.1
wget -q --no-check-certificate https://storage.googleapis.com/tensorflow/versions/2.18.0/${FILENAME}
sudo tar -C /usr/local -xzf ${FILENAME}
```

### crow

```bash
wget https://github.com/CrowCpp/Crow/releases/download/v1.2.0/Crow-1.2.0-Linux.deb 
dpkg -i Crow-1.2.0-Linux.deb
cp -r /usr/include/crow src/include
sudo rm -rf /usr/include/crow
```

### things may help you in development

everything is pretty straightforward, factory default QT and other libraries. i will add "watch out for this" if i have any of them:

#### generate binary headers for embedding assets

```bash
assets/asset_converter.py assets/no_cover_art.gif NoCoverArtGif > src/include/assets/no_cover_art.h
```


```cpp
#include "assets/no_cover_art.h"
#include <QMovie>
#include <QByteArray>

QByteArray gifData(reinterpret_cast<const char*>(Resources::NoCoverArtGif), Resources::noCoverArtGif_size);
QMovie *movie = new QMovie();
movie->setDevice(new QBuffer(&gifData));
movie->start();

QLabel *label = new QLabel();
label->setMovie(movie);
```

#### vscode setup

c/c++ official Microsoft extension's LSP is absolutely horrible thanks to including bajillions of headers from ffmpeg, QT and Essentia, sometimes taking four seconds to reanalyze a single header file. install clangd extension (on neovim and zed, clang is the default LSP)

use c/c++ only for debugging, so your workspace `settings.json` should contain the following:
```json
  "clangd.arguments": [
    "-log=verbose",
    "-pretty",
    "--background-index",
    "--compile-commands-dir=build/native"
  ],
  "C_Cpp.intelliSenseEngine": "disabled",
  "C_Cpp.autocomplete": "disabled",
  "C_Cpp.errorSquiggles": "disabled",
  "C_Cpp.formatting": "disabled",
  "[cpp]": {
    "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"
  },
  "maptz.regionfolder": {
    "[cpp]": {
      "foldEndRegex": "^[\\s]*#pragma[\\s]*endregion.*$",
      "foldStartRegex": "^[\\s]*#pragma[\\s]*region[\\s]*(.*)[\\s]*$"
    },
},
```
`compile-commands-dir` should point to the build directory as CMake drops the `compile_commands.json` right there.


if you want to use `#pragma region`, install [#region folding](https://marketplace.visualstudio.com/items?itemName=maptz.regionfolder) extension, because clangd does not support [pragma blocks](https://github.com/clangd/clangd/issues/1623) and add this to your configuration:
```json
"maptz.regionfolder": {
    "[cpp]": {
      "foldEndRegex": "^[\\s]*#pragma[\\s]*endregion.*$",
      "foldStartRegex": "^[\\s]*#pragma[\\s]*region[\\s]*(.*)[\\s]*$"
    }}
```
if you have any errors upon Debug on CMake extension, also add this
```json
  "cmake.debugConfig": {
    "MIMode": "gdb",
    "miDebuggerPath": "/usr/bin/gdb"
  },
```
## why

### why do you have `time.h` at include folder

ffmpeg's libavcodec library has `time.h` header that clashes with system default `time.h` and if you dont include the system `time.h` before including the libavcodec headers, everything goes boom. 


### why did you create this project

This project is born with the jealousy of infinite music queueing algorithm in Apple Music's stations, I will write a detailed "why" at the end of the project, but at its core, I am just in awe of how good Apple Music stations works, and how I can listen hours of music due to how good all songs blend in with each other. Trying to replicate with small library of mine.


## problems, todos, and many more rants

issues never ends...

### cannot link tensorflow and tensorflow framework

```bash 
➜  conan git:(cpp) ✗ lldb-19 build/Conan
(lldb) target create "build/Conan"
Current executable set to '/home/cansu/Git/conan/build/Conan' (x86_64).
(lldb) run
Process 69396 launched: '/home/cansu/Git/conan/build/Conan' (x86_64)
Failed to create wl_display (No such file or directory) # irrelevant
qt.qpa.plugin: Could not load the Qt platform plugin "wayland" in "" even though it was found. # irrelevant
Process 69396 stopped
* thread #1, name = 'Conan', stop reason = signal SIGSEGV: address not mapped to object (fault address: 0x8)
    frame #0: 0x00007fffbdeb1ff1 libtensorflow_framework.so.2`llvm::raw_svector_ostream::write_impl(char const*, unsigned long) + 17
libtensorflow_framework.so.2`llvm::raw_svector_ostream::write_impl:
->  0x7fffbdeb1ff1 <+17>: movq   0x8(%r14), %rdi
    0x7fffbdeb1ff5 <+21>: addq   %rdi, %rdx
    0x7fffbdeb1ff8 <+24>: cmpq   %rdx, 0x10(%r14)
    0x7fffbdeb1ffc <+28>: jae    0x7fffbdeb201c ; <+60>
```

which, both libraries work fine with a sample test code
```c
#include <stdio.h>
#include <tensorflow/c/c_api.h>

int main() {
  printf("Hello from TensorFlow C library version %s\n", TF_Version());
  return 0;
}
```

```bash
gcc hello_tf.c -ltensorflow -o hello_tf
./hello_tf
Hello from TensorFlow C library version 2.18.0
```

this will be hell to debug, but we have installed and linked all the libraries we need, so if I fix TF, there will be no compile issues left with CMake.

### windows and arm toolchains (mingw, etc) does not work. 

need to rebuild the incompatible libraries in both windows and mac.  for example, when configured with `GCC 13-win32 x86_64-w64-mingw32` kit:

```
[cmake] CMake Error at /usr/share/cmake-3.28/Modules/FindPackageHandleStandardArgs.cmake:230 (message):
[cmake]   Could NOT find ZLIB (missing: ZLIB_LIBRARY) (found version "1.3")
[cmake] Call Stack (most recent call first):
[cmake]   /usr/share/cmake-3.28/Modules/FindPackageHandleStandardArgs.cmake:600 (_FPHSA_FAILURE_MESSAGE)
[cmake]   /usr/share/cmake-3.28/Modules/FindZLIB.cmake:199 (FIND_PACKAGE_HANDLE_STANDARD_ARGS)
[cmake]   /usr/share/cmake-3.28/Modules/CMakeFindDependencyMacro.cmake:76 (find_package)
[cmake]   /usr/lib/cmake/Crow/CrowConfig.cmake:42 (find_dependency)
[cmake]   CMakeLists.txt:34 (find_package)
```

so we cannot even configure the CMake with other kits right now. both `Clang 19.1.6 x86_64-pc-linux-gnu` and `GCC 13.3.0 x86_64-pc-linux-gnu` works for Linux, I will sort out something for both Win and OSX after all the compiling pain (see above for tensorflow disaster) for Linux ends.
