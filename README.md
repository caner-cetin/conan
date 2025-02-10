## conan
smol music recommendation engine based on local library folders. (**WORK IN PROGRESS**)

![alt text](./static/ui.png)

*ui is not final, screenshot from python branch*

- [conan](#conan)
- [how does the recommendation algorithm works?](#how-does-the-recommendation-algorithm-works)
- [features](#features)
- [download](#download)
  - [foobar2000](#foobar2000)
- [compile yourself](#compile-yourself)
  - [try package repos first](#try-package-repos-first)
  - [use clang](#use-clang)
  - [dependencies](#dependencies)
  - [things may help you in development](#things-may-help-you-in-development)
- [why](#why)
  - [why do you have `time.h` at include folder](#why-do-you-have-timeh-at-include-folder)
  - [why dont you use qtwebengine](#why-dont-you-use-qtwebengine)
  - [why did you create this project](#why-did-you-create-this-project)
- [problems, todos, and many more rants](#problems-todos-and-many-more-rants)
  - [Gtk-WARNING \*\*: cannot open display on macos](#gtk-warning--cannot-open-display-on-macos)
  - [windows toolchains (mingw, etc) does not work.](#windows-toolchains-mingw-etc-does-not-work)
- [closing thoughts](#closing-thoughts)

## how does the recommendation algorithm works?

i didnt do the math, [someone else did](https://www.researchgate.net/publication/351863177_Content-based_Music_Recommendation_System), and I tried my best to explain how it works with examples [in this header file](src/workers/analysis.h).

## features
WIP
## download
### foobar2000
install [beefweb from here](https://github.com/hyperblast/beefweb/releases/tag/v0.8). 
then from f2k, files -> settings -> tools -> beefweb remote control -> tick allow remote connections and set port.

settings WIP

## compile yourself
trust me, this is not worth your time. most depndencies are compiled from source, and it will take so much time from you, i am talking about several hours. if you follow these steps, and for uncertain reasons you decide to compile this application, you are my best friend from now on. 

### try package repos first

for QT and webkitgtk, try installing them from your package repository first. if they dont work, compile yourself.

### use clang
export the folloving envs
```bash
export CC=/usr/bin/clang-19
export CXX=/usr/bin/clang++-19
```
until we are done
### dependencies
<details>

<summary>libs</summary>

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
    libjemalloc-dev \
    libutfcpp-dev \
    libssl-dev \
    uwsgi \
    zlib1g-dev \
    ruby-full \ 
    ccache
```

</details>

<details>

<summary>tensorflow</summary>

x86 linux

```bash
wget -q https://storage.googleapis.com/tensorflow/libtensorflow/libtensorflow-gpu-linux-x86_64-2.12.1.tar.gz  
mkdir -p src/vendor/tensorflow
sudo tar -C src/vendor/tensorflow -xzf libtensorflow-gpu-linux-x86_64-2.12.1.tar.gz
```

for silicon mac, just `brew install libtensorflow`

</details>


<details>

<summary>qt</summary>

```bash
sudo apt install \
    ninja-build \
    libfontconfig1-dev \
    libfreetype-dev \
    libx11-dev \
    libx11-xcb-dev \
    libxcb-cursor-dev \
    libxcb-glx0-dev \
    libxcb-icccm4-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-randr0-dev \
    libxcb-render-util0-dev \
    libxcb-shape0-dev \
    libxcb-shm0-dev \
    libxcb-sync-dev \
    libxcb-util-dev \
    libxcb-xfixes0-dev \
    libxcb-xinerama0-dev \
    libxcb-xkb-dev \
    libxcb1-dev \
    libxext-dev \
    libxfixes-dev \
    libxi-dev \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libxrender-dev \
    libcups2-dev \
    libdrm-dev \
    libegl1-mesa-dev \
    libnss3-dev \
    libpci-dev \
    libpulse-dev \
    libudev-dev \
    libxtst-dev \
    libasound2-dev \
    libinput-dev \
    libclang-19-dev \
    llvm-19-dev \
    libseccomp-dev \
    libseccomp2 \
    gettext
```
```bash
wget https://download.qt.io/official_releases/qt/6.8/6.8.1/single/qt-everywhere-src-6.8.1.tar.xz -C ~/Downloads/qt-everywhere-src-6.8.1.tar.xz
cd /tmp
tar xf ~/Downloads/qt-everywhere-src-6.8.1.tar.xz
mkdir -p ~/dev/qt-build
cd ~/dev/qt-build
/tmp/qt-everywhere-src-6.8.1/qtbase/configure -top-level -debug-and-release -skip qtmultimedia -skip qtquick3dphysics -skip qtremoteobjects -skip qtvirtualkeyboard -skip qtpositioning -skip qtspeech -skip qt3d -skip qtquick3d -skip qtlanguageserver -skip qtdatavis3d -skip qtlocation -skip qtgrpc -skip qtcoap -skip qtopcua -skip qtmqtt -skip qtsensors -skip qtgraphs -skip qtconnectivity -skip qtlottie -skip qtnetworkauth -skip qtdoc -skip qtscxml -skip qtwebchannel -skip qtwebengine -skip qtwebview -skip qthttpserver -skip qtwebsockets -skip qtcharts -skip qtactiveqt  -skip-tests qtbase,qt5compat,qtimageformats,qtshadertools,qtmultimedia,qtserialport,qtserialbus,qtsvg,qttools,qttranslations,qtwayland -skip-examples qtbase,qt5compat,qtimageformats,qtshadertools,qtmultimedia,qtserialport,qtserialbus,qtsvg,qttools,qttranslations,qtwayland -gui -widgets
cmake --build . --parallel $(nproc)
sudo cmake --install .
```
and then
```
...
After Qt is installed, you can start building applications with it.
If you work from the command line, consider adding the Qt tools to your default PATH. This is done as follows:
In .profile (if your shell is bash, ksh, zsh or sh), add the following lines:

PATH=/usr/local/Qt-6.8.1/bin:$PATH
export PATH

In .login (if your shell is csh or tcsh), add the following line:

setenv PATH /usr/local/Qt-6.8.1/bin:$PATH
```

</details>

<details>

<summary>ffmpeg</summary>

grab any version 5 from your package manager

</details>

<details>

<summary>essentia</summary>

```bash
git clone https://github.com/wo80/essentia.git
cd essentia
git checkout cmake
# adjust the architecture
python3 waf configure --build-static --with-tensorflow --mode=release CXXFLAGS="-arch arm64" LINKFLAGS="-arch arm64" --lightweight=libav,libsamplerate,taglib,yaml,fftw,libchromaprint
python3 waf
sudo python3 waf install
```

</details>

<details>

<summary>glslang</summary>

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

</details>

<details>

<summary>gtk & webkitgtk</summary>

gtk 3
```bash
wget https://download.gnome.org/sources/gtk+/3.24/gtk+-3.24.34.tar.xz
tar -xf gtk+-3.24.34.tar.xz
cd gtk+-3.24.34
mkdir build
cd build
meson --prefix=/usr --buildtype=release -Dintrospection=false -Ddemos=false -Dexamples=false -Dtests=false ..
ninja
sudo ninja install
```
webkitgtk
```bash
wget https://webkitgtk.org/releases/webkitgtk-2.46.5.tar.xz
tar -xf webkitgtk-2.46.5.tar.xz
cd webkitgtk-2.46.5
mkdir build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DPORT=GTK -DENABLE_INTROSPECTION=OFF -DENABLE_GTKDOC=OFF -DENABLE_MINIBROWSER=OFF -DENABLE_GAMEPAD=OFF -DENABLE_WAYLAND_TARGET=OFF -DUSE_AVIF=OFF -DENABLE_JOURNALD_LOG=OFF -DUSE_LCMS=OFF -DUSE_GSTREAMER_TRANSCODER=OFF -DENABLE_TOUCH_EVENTS=OFF -DUSE_GTK4=OFF -DUSE_JPEGXL=OFF -S . -B build
cd build
ninja
sudo ninja install
```
</details>

<details>

<summary>crow</summary>

```bash
wget https://github.com/CrowCpp/Crow/releases/download/v1.2.0/Crow-1.2.0-Linux.deb 
dpkg -i Crow-1.2.0-Linux.deb
cp -r /usr/include/crow src/include
sudo rm -rf /usr/include/crow
```

</details>

<details>

<summary>taglib</summary>

```bash
wget https://taglib.org/releases/taglib-2.0.2.tar.gz
tar -xf taglib-2.0.2.tar.gz
cd taglib-2.0.2
cmake -DCMAKE_BUILD_TYPE=Release . 
make
sudo make install
```

</details>

### things may help you in development

<details>

<summary>generate binary headers for embedding assets</summary>

```bash
generate_assets.py assets/no_cover_art.gif NoCoverArtGif > src/include/assets/no_cover_art.h
```

to generate everything
```bash
chmod +x generate_assets.sh
./generate_assets.py
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

</details>

<details>

<summary>vscode setup</summary>

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

</details>

## why

### why do you have `time.h` at include folder

ffmpeg's libavcodec library has `time.h` header that clashes with system default `time.h` and if you dont include the system `time.h` before including the libavcodec headers, everything goes boom. 

### why dont you use qtwebengine

i hate it, and my anger is still echoed across the continents. I used QTWebEngine at first, and, due to me being forced to recompile QT, I had to recompile QTWebEngine again. Holy fucking shit. I have no words just check this series of posts.

https://forums.gentoo.org/viewtopic-p-8651147.html?sid=1773648ee041db179d6a8afd6519c3d8 
https://forums.gentoo.org/viewtopic-p-8799327.html?sid=5a2879e47da1e0b4a9c6ae1941e8eb2a 
https://forums.gentoo.org/viewtopic-p-8823870.html?sid=373baff52f1adbc6379341f9c056b538 
https://forums.gentoo.org/viewtopic-p-8786184.html?sid=c5d84f39b263a35504fdd4c0ffbe2c5e 
https://forums.gentoo.org/viewtopic-p-8584839.html?sid=43aad284c43073a22c37b83ceaa01de8

Compiling QTWebEngine is borderline impossible for my machine. So we just embed a browser into QWindow instead. 



### why did you create this project

This project is born with the jealousy of infinite music queueing algorithm in Apple Music's stations, I will write a detailed "why" at the end of the project, but at its core, I am just in awe of how good Apple Music stations works, and how I can listen hours of music due to how good all songs blend in with each other. Trying to replicate with small library of mine.


## problems, todos, and many more rants

issues never ends...

### Gtk-WARNING **: cannot open display on macos

install https://www.xquartz.org then relaunch the app

### windows toolchains (mingw, etc) does not work. 

makefiles in https://github.com/mxe/mxe will be used later to compile for windows

## closing thoughts

i love you
