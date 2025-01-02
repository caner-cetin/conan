## conan
smol music recommendation engine based on local library folders.

- [conan](#conan)
- [features](#features)
- [download](#download)
- [compile yourself](#compile-yourself)
  - [libs](#libs)
  - [ffmpeg](#ffmpeg)
  - [essentia](#essentia)
  - [glslang](#glslang)
  - [gtest](#gtest)
  - [things may help you in development](#things-may-help-you-in-development)
- [why](#why)
  - [why do you have `time.h` at include folder](#why-do-you-have-timeh-at-include-folder)
  - [why did you create this project](#why-did-you-create-this-project)
- [todo](#todo)


![alt text](image.png)
*ui is not final*
## features
zzz...
## download
there is a x86_64 Linux binary in releases. i will compile for windows (as, it is the main reason why i use C++ ), but you can use like how I do now. Install WSL2, install Ubuntu 24.04 or any distro you prefer, run the binary, you will use the application like it is on native Linux desktop.
## compile yourself
trust me, this is not worth your time, and do not follow this. if you follow these steps, and for uncertain reasons you decide to compile this application, you are my best friend from now on.

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
    libglslang-dev \
    libeigen3-dev \
    libfftw3-dev \
    libchromaprint-dev \
    libspdlog-dev \
    libfmt-dev \
    libtagc0-dev \
    libva-drm2 \
    libbz2-dev \
    liblzma-dev \
    zlib1g-dev \
    qt6-base-dev \
    qt6-base-dev-tools
```
### ffmpeg
this is gonna override your default ffmpeg installation, but you can bump to latest version after compiling essentia with your package manager like `apt-get install ffmpeg` 
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

everything started from this [small comment](<https://github.com/MTG/essentia/issues/1411#issuecomment-2564929319)>), and slowly devolved into insanity.

### things may help you in development

everything is pretty straightforward, factory default QT and other libraries. i will add "watch out for this" if i have any of them:

- generate binary headers for embedding assets

```bash
assets/asset_converter.py assets/no_cover_art.gif noCoverArtGif > src/include/assets/no_cover_art.h
```


```cpp
#include "assets/no_cover_art.h"
#include <QMovie>
#include <QByteArray>

QByteArray gifData(reinterpret_cast<const char*>(Resources::noCoverArtGif), Resources::noCoverArtGif_size);
QMovie *movie = new QMovie();
movie->setDevice(new QBuffer(&gifData));
movie->start();

QLabel *label = new QLabel();
label->setMovie(movie);
```

## why

### why do you have `time.h` at include folder

ffmpeg's libavcodec library has `time.h` header that clashes with system default `time.h` and if you dont include the system `time.h` before including the libavcodec headers, everything goes boom. 

### why did you create this project

i have no idea but i will get back to you with a reason soon.


## todo

- queue / play for similar tracks

- marquee effect for long labels in lists

with long track titles such as 
```
"GOVERNMENT CAME" (9980.0kHz 3617.1kHz 4521.0 kHz) / Cliffs Gaze / cliffs' gaze at empty waters' rise / ASHES TO SEA or NEARER TO THEE
```
labels wraps without preserving the value before being wrapped:

![alt text](./static/t1.png)

and search is broken afterwards:

![alt text](./static/t2.png)

where it works fine if searched by wrapped text:

![alt text](./static/t3.png)

- ???
