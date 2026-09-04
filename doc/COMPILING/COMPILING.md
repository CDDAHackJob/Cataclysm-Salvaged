# Compiling
* [General Linux Guide](#general-linux-guide)
  * [Compiler](#compiler)
  * [Tools](#tools)
  * [Dependencies](#dependencies)
  * [Make flags](#make-flags)
  * [Compiling localization files](#compiling-localization-files)
* [Fedora](#fedora)
  * [ncurses builds](#ncurses-builds)
  * [SDL builds](#sdl-builds)
* [Debian](#debian)
  * [Linux (native) ncurses builds](#linux-native-ncurses-builds)
  * [Linux (native) SDL builds](#linux-native-sdl-builds)
  * [Cross-compiling to linux 32-bit from linux 64-bit](#cross-compiling-to-linux-32-bit-from-linux-64-bit)
  * [Cross-compile to Windows from Linux](#cross-compile-to-windows-from-linux)
* [Windows](#windows)
  * [Building with Visual Studio](#building-with-visual-studio)
  * [Building with MSYS2](#building-with-msys2)
  * [Building with CYGWIN](#building-with-cygwin)
  * [Building with Clang and MinGW64](#building-with-clang-and-mingw64)
* [BSDs](#bsds)

# General Linux Guide

To build Cataclysm from source you will need at least a C++ compiler, some basic developer tools, and necessary build dependencies. The exact package names vary greatly from distro to distro, so this part of the guide is intended to give you higher-level understanding of the process.

## Compiler

You have three major choices here: GCC, Clang and MXE.

  * GCC is almost always the default on Linux systems so it's likely you already have it
  * Clang is usually faster than GCC, so it's worth installing if you plan to keep up with the latest experimentals
  * MXE is a cross-compiler, so of any importance only if you plan to compile for Windows on your Linux machine

(Note that your distro may have separate packages e.g. `gcc` only includes the C compiler and for C++ you'll need to install `g++`.)

Cataclysm is targeting the C++17 standard and that means you'll need a compiler that supports it. You can check if your C++ compiler supports the standard by reading [COMPILER_SUPPORT.md](./COMPILER_SUPPORT.md)

The general rule is the newer the compiler the better.

## Tools

Most distros seem to package essential build tools as either a single package (Debian and derivatives have `build-essential`) or a package group (Arch has `base-devel`). You should use the above if available. Otherwise you'll at least need `make` and figure out the missing dependencies as you go (if any).

Besides the essentials you will need `git`.

If you plan on keeping up with experimentals you should also install `ccache`, which  will considerably speed-up partial builds.

## Dependencies

There are some general dependencies, optional dependencies, and then specific dependencies for either curses or tiles builds. The exact package names again depend on the distro you're using, and whether your distro packages libraries and their development files separately (e.g. Debian and derivatives).

Rough list based on building on Arch:

  * General: `gcc-libs`, `glibc`, `zlib`, `bzip2`
  * Optional: `gettext`
  * Curses: `ncurses`
  * Tiles: `sdl2`, `sdl2_image`, `sdl2_ttf`, `sdl2_mixer`, `freetype2`

E.g. for curses build on Debian and derivatives you'll also need `libncurses5-dev` or `libncursesw5-dev`.

Note on optional dependencies:

  * `gettext` - for localization support; if you plan to only use English you can skip it

You should be able to figure out what you are missing by reading the compilation errors and/or the output of `ldd` for compiled binaries.

## Make flags

Given you're building from source you have a number of choices to make:

  * `NATIVE=` - you should only care about this if you're cross-compiling
  * `RELEASE=1` - without this you'll get a debug build (see note below)
  * `LTO=1` - enables link-time optimization with GCC/Clang
  * `TILES=1` - with this you'll get the tiles version, without it the curses version
  * `SOUND=1` - if you want sound; this requires `TILES=1`
  * `LOCALIZE=0` - this disables localizations so `gettext` is not needed
  * `CLANG=1` - use Clang instead of GCC
  * `CCACHE=1` - use ccache
  * `USE_LIBCXX=1` - use libc++ instead of libstdc++ with Clang (default on OS X)
  * `PREFIX=DIR` - causes `make install` to place binaries and data files to DIR (see note below)

There is a couple of other possible options - feel free to read the [`Makefile`](../../Makefile).

If you have a multi-core computer you'd probably want to add `-jX` to the options, where `X` is your CPU's number of threads (generally twice the number of your CPU cores). Alternatively, you can add `-j$(nproc)` for the build to use all of your CPU processors.

Examples:
- `make -j4 CLANG=1 CCACHE=1 NATIVE=linux64 RELEASE=1 TILES=1`
- `make -j$(nproc) CLANG=1 TILES=1 SOUND=0`

The above will build a tiles release explicitly for 64 bit Linux, using Clang and ccache and 4 parallel processes.

Example: `make -j2 LOCALIZE=0`

The above will build a debug-enabled curses version for the architecture you are using, using GCC and 2 parallel processes.

**Note on debug**:
You should probably always build with `RELEASE=1` unless you experience segfaults and are willing to provide stack traces.

**Note on PREFIX**:
PREFIX specifies a directory which will be the prefix for binaries, resources, and documentation files. Compiling with PREFIX means cataclysm will read files from PREFIX directory. This can be overridden with `--datadir` (e.g. if you used `PREFIX=DIR` in earlier build, then specify `--datadir DIR/share/cataclysm-dda`).

## Compiling localization files

If you want to compile localization files for specific languages, you can add the `LANGUAGES="<lang_id_1> [lang_id_2] [...]"` option to the `make` command:

    make LANGUAGES="zh_CN zh_TW"

You can get the language ID from the filenames of `*.po` in `lang/po` directory. Setting `LOCALIZE=1` only may not tell `make` to compile those localization files for you.

# Accelerating Linux builds with llama

[llama](https://github.com/nelhage/llama) is a CLI tool for outsourcing computation to AWS Lambda.  If you want your builds to run faster and are willing to pay Amazon for the privilege, then you may be able to use it to accelerate your builds.  See [our llama README](../../tools/llama/README.md) for more details.

# Fedora
## Ncurses builds

Dependencies:

  * ncurses
  * g++ and make

Install:

    sudo dnf install astyle gcc-c++ ncurses-devel make

### Building

Run:

    make

## SDL builds

Dependencies:

  * SDL2
  * SDL2_image
  * SDL2_ttf
  * freetype
  * g++ and make
  * libsdl2-mixer-dev - Used if compiling with sound support.

Install:

    sudo dnf install astyle gcc-c++ SDL2-devel SDL2_image-devel SDL2_ttf-devel SDL2_mixer-devel freetype-devel make

### Building

A simple installation could be done by simply running:

    make TILES=1

A more comprehensive alternative is:

    make -j2 TILES=1 SOUND=1 RELEASE=1 USE_HOME_DIR=1

The -j2 flag means it will compile with two parallel processes. It can be omitted or changed to -j4 in a more modern processor. If there is no desire to have sound, those flags can also be omitted. The USE_HOME_DIR flag places the user files, like configurations and saves, into the home folder, making it easier for backups, and can also be omitted.

# Gentoo
If you want sound and graphics, make sure to emerge with the following:

```bash
USE="flac fluidsynth mad midi mod modplug mp3 playtools vorbis wav png" \
 emerge -1va emerge media-libs/libsdl2 media-libs/sdl2-gfx media-libs/sdl2-image media-libs/sdl2-mixer media-libs/sdl2-ttf
```

It may also be possible to get away with fewer dependencies, but this set has been tested.

Once the above libraries are installed, compile with:

    make -j$(nproc) TILES=1 SOUND=1 RELEASE=1


# Debian

Instructions for compiling on a Debian-based system. The package names here are valid for Ubuntu 12.10 and may or may not work on your system.

The building instructions below always assume you are running them from the Cataclysm:DDA source directory.


## Linux (native) ncurses builds

Dependencies:

  * ncurses or ncursesw (for multi-byte locales)
  * build essentials

Install:

    sudo apt-get install libncurses5-dev libncursesw5-dev build-essential astyle

### Building

Run:

    make

## Linux (native) SDL builds

Dependencies:

  * SDL
  * SDL_ttf
  * freetype
  * build essentials
  * libsdl2-mixer-dev - Used if compiling with sound support.

Install:

    sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev libfreetype6-dev build-essential

### Building

A simple installation could be done by simply running:

    make TILES=1

A more comprehensive alternative is:

    make -j2 TILES=1 SOUND=1 RELEASE=1 USE_HOME_DIR=1

The -j2 flag means it will compile with two parallel processes. It can be omitted or changed to -j4 in a more modern processor. If there is no desire to have sound, those flags can also be omitted. The USE_HOME_DIR flag places the user files, like configurations and saves, into the home folder, making it easier for backups, and can also be omitted.



## Cross-compiling to linux 32-bit from linux 64-bit

Dependencies:

  * 32-bit toolchain
  * 32-bit ncursesw (compatible with both multi-byte and 8-bit locales)

Install:

    sudo apt-get install libc6-dev-i386 lib32stdc++-dev g++-multilib lib32ncursesw5-dev

### Building

Run:

    make NATIVE=linux32

## Cross-compile to Windows from Linux

To cross-compile to Windows from Linux, you will need MXE, which changes your `make` command slightly. These instructions were written from Ubuntu 20.04, but should be applicable to any Debian-based environment. Please adjust all package manager instructions to match your environment.

Dependencies:

  * [MXE](http://mxe.cc)
  * [MXE Requirements](http://mxe.cc/#requirements)

Installation

<!-- astyle and lzip added to initial sudo apt install string to forestall complaints from MinGW and make -->
<!-- ncurses removed from make MXE_TARGETS because we're not gonna be cross-compiling ncurses -->

```bash
sudo apt install astyle autoconf automake autopoint bash bison bzip2 cmake flex gettext git g++ gperf intltool libffi-dev libgdk-pixbuf2.0-dev libtool libltdl-dev libssl-dev libxml-parser-perl lzip make mingw-w64 openssl p7zip-full patch perl pkg-config python3 ruby scons sed unzip wget xz-utils g++-multilib libc6-dev-i386 libtool-bin
mkdir -p ~/src/libbacktrace
cd ~/src
git clone https://github.com/CDDAHackJob/Cataclysm-Salvaged.git
git clone https://github.com/mxe/mxe.git
cd mxe
make -j$((`nproc`+0)) MXE_TARGETS='x86_64-w64-mingw32.static i686-w64-mingw32.static' MXE_PLUGIN_DIRS=plugins/gcc11 sdl2 sdl2_ttf sdl2_image sdl2_mixer gettext
cd ../libbacktrace/
wget https://github.com/Qrox/libbacktrace/releases/download/2020-01-03/libbacktrace-x86_64-w64-mingw32.tar.gz
wget https://github.com/Qrox/libbacktrace/releases/download/2020-01-03/libbacktrace-i686-w64-mingw32.tar.gz
tar -xzf libbacktrace-x86_64-w64-mingw32.tar.gz --exclude=LICENSE -C ~/src/mxe/usr/x86_64-w64-mingw32.static
tar -xzf libbacktrace-i686-w64-mingw32.tar.gz --exclude=LICENSE -C ~/src/mxe/usr/i686-w64-mingw32.static
```

Building all these packages from MXE might take a while, even on a fast computer. Be patient; the `-j` flag will take advantage of all your processor cores. If you are not planning on building for both 32-bit and 64-bit, you might want to adjust your MXE_TARGETS.  Additionally if not building for a particular target you can skip the curl and tar commands for the targets NOT being built.

An additional note: With C:DDA switching to gcc 11.2 with MXE (MingW), if you've previously built MXE you'll need to "make clean" and rebuild it to get gcc11.

Edit your `~/.profile` as follows:

```bash
export PLATFORM_32="~/src/mxe/usr/bin/i686-w64-mingw32.static-"
export PLATFORM_64="~/src/mxe/usr/bin/x86_64-w64-mingw32.static-"
```

This is to ensure that the variables for the `make` command will not get reset after a power cycle.

### Building (SDL)

    cd ~/src/Cataclysm-Salvaged

Run one of the following commands based on your targeted environment:

```bash
make -j$((`nproc`+0)) CROSS="${PLATFORM_32}" TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 bindist
make -j$((`nproc`+0)) CROSS="${PLATFORM_64}" TILES=1 SOUND=1 RELEASE=1 LOCALIZE=1 bindist
```


<!-- Building ncurses for Windows is a nonstarter, so the directions were removed. -->

# Windows

## Building with Visual Studio

See [COMPILING-VS-VCPKG.md](COMPILING-VS-VCPKG.md) for instructions on how to set up and use a build environment using Visual Studio on windows.

This is probably the easiest solution for someone used to working with Visual Studio and similar IDEs.

For an alternative setup using [CMake](../../CMakeLists.txt), please read [COMPILING-CMAKE-VCPKG.md](COMPILING-CMAKE-VCPKG.md).

## Building with MSYS2

See [COMPILING-MSYS.md](COMPILING-MSYS.md) for instructions on how to set up and use a build environment using MSYS2 on windows.

MSYS2 strikes a balance between a native Windows application and a UNIX-like environment. There's some command-line tools that our project uses (notably our JSON linter) that are harder to use without a command-line environment such as what MSYS2 or CYGWIN provide.

## Building with CYGWIN

See [COMPILING-CYGWIN.md](COMPILING-CYGWIN.md) for instructions on how to set up and use a build environment using CYGWIN on windows.

CYGWIN attempts to more fully emulate a POSIX environment, to be "more unix" than MSYS2. It is a little less modern in some respects, and lacks the convenience of the MSYS2 package manager.

## Building with Clang and MinGW64

Clang by default uses MSVC on Windows, but also supports the MinGW64 library. Simply replace `CLANG=1` with `"CLANG=clang++ -target x86_64-pc-windows-gnu -pthread"` in your batch script, and make sure MinGW64 is in your path. You may also need to apply [a patch](https://sourceforge.net/p/mingw-w64/mailman/message/36386405/) to `float.h` of MinGW64 for the unit test to compile.

# BSDs

There are reports of CDDA building fine on recent OpenBSD and FreeBSD machines (with appropriately recent compilers), and there is some work being done on making the [`Makefile`](../../Makefile) "just work", however we're far from that and BSDs support is mostly based on user contributions. Your mileage may vary. So far essentially all testing has been on amd64, but there is no (known) reason that other architectures shouldn't work, in principle.

### Building on FreeBSD/amd64 13.0 with the system compiler

FreeBSD uses clang as the default compiler, which provides C++17 support out of the box since FreeBSD 12.0.

Install the following with pkg (or from Ports):

    pkg install gmake libiconv

Tiles builds will also require SDL2:

    pkg install sdl20 sdl2_image sdl2_mixer sdl2_ttf

Then you should be able to build with something like this:

```bash
gmake RELEASE=1 # ncurses builds
gmake RELEASE=1 TILES=1 # tiles builds
```

### Building on OpenBSD/amd64 7.1 with clang

Install necessary dependencies:

    pkg_add gmake libiconv

If building with tiles also install:

    pkg_add sdl2 sdl2-image sdl2-mixer sdl2-ttf

Compiling:

```bash
gmake RELEASE=1 BSD=1 CLANG=1 TESTS=0 # ncurses build
gmake RELEASE=1 BSD=1 CLANG=1 TESTS=0 TILES=1 # tiles build
```

You may get an out of memory error when compiling with an underprivileged user,
you can simply fix this by running:

    usermod -L pbuild <user>

This command sets your login class to `pbuild` thus the data ulimit is increased from
1GB to 8GB.

**NOTE: don't run this command for your main user, as it is already a part
of the `staff` login class**

Instead, you need to increase `data` limit with:

    ulimit -d 8000000

### Building on OpenBSD/amd64 5.8 with GCC 4.9.2 from ports/packages

First, install g++, gmake, and libexecinfo from packages (g++ 4.8 or 4.9 should work; 4.9 has been tested):

    pkg_add g++ gmake libexecinfo

Then you should  be able to build with something like:

    CXX=eg++ gmake

Only an ncurses build is possible on 5.8-release, as SDL2 is broken. On recent -current or snapshots, however, you can install the SDL2 packages:

    pkg_add sdl2 sdl2-image sdl2-mixer sdl2-ttf

and build with:

    CXX=eg++ gmake TILES=1

### Building on NetBSD/amd64 7.0RC1 with the system compiler

NetBSD has (or will have) gcc 4.8.4 as of version 7.0, which is new enough to build cataclysm. You will need to install gmake and ncursesw:

    pkgin install gmake ncursesw

Then you should be able to build with something like this (LDFLAGS for ncurses builds are taken care of by the ncurses configuration script; you can of course set CXXFLAGS/LDFLAGS in your .profile or something):

```bash
export CXXFLAGS="-I/usr/pkg/include"
gmake # ncurses builds
LDFLAGS="-L/usr/pkg/lib" gmake TILES=1 # tiles builds
```

SDL builds currently compile, but did not run in my testing - not only do they segfault, but gdb segfaults when reading the debug symbols! Perhaps your mileage will vary.
