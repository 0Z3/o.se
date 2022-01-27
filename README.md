o.se is a command-line interface and runtime server
environment for
[OpenSoundControl](https://opensoundcontrol.stanford.edu).

# Documentation

Documentation for the o.se command-line tool:
[https://0Z3.github.com/o.se](https://0Z3.github.com/o.se)

General Ose documentation, and links to other o.se hosts: 
[https://0Z3.github.com/](https://0Z3.github.com/)

# Download

Precompiled binares for a variety of platforms (macOS, Linux,
Windows, Raspberry Pi, and others) are available here:

[https://github.com/0Z3/o.se/releases](https://github.com/0Z3/o.se/releases)

# Quickstart

```
cd <path/to/o.se>
./o.se -f repl.ose
/s/Hello World!
/!/println
```

# Building from source

Download or clone 
[https://github.com/0Z3/o.se](https://github.com/0Z3/o.se).

If cloning, make sure to update the submodules:

    git submodule update --init --recursive

## Mac OS X

### Prerequisites

* cmake (`brew install cmake`)

```
git clone https://github.com/0Z3/o.se.git
cd o.se
git submodule update --init --recursive
make all
# or
# make all-debug
```

## Ubuntu, Debian, Raspberry Pi OS, etc.

```
apt install git clang make cmake uuid-dev
git clone https://github.com/0Z3/o.se.git
cd o.se
git submodule update --init --recursive
make all
# or 
# make all-debug
```

## Arch Linux

```
pacman -S git clang make cmake fakeroot patch pkgconfig
git clone https://aur.archlinux.org/uuid.git
cd uuid
makepkg -si
cd ..
git clone https://github.com/0Z3/o.se.git
cd o.se
git submodule update --init --recursive
make all
# or 
# make all-debug
```

## FreeBSD

```
pkg install git gmake cmake 
git clone https://github.com/0Z3/o.se.git
git submodule update --init --recursive
gmake all
# or
# gmake all-debug
```

## Windows (MSYS2)

### Prerequisites

* [MSYS2](https://www.msys2.org)

```
pacman -S git mingw-w64-x86_64-toolchain make cmake libutil-linux-devel
git clone https://github.com/0Z3/o.se.git
cd o.se
git submodule update --init --recursive
CCOMPILER=gcc CPPCOMPILER=g++ make all
# or
# CCOMPILER=gcc CPPCOMPILER=g++ DEBUG_SYMBOLS=gdb make all-debug
```

## Make Targets

### Release

Release mode means all optimizations on (`-O3`), debugging
code removed, and no debugging symbols.

`make` : builds o.se in release mode

`make all-modules` : builds all modules in release mode

`make all` : builds o.se and all modules in release mode

### Debug

Debug mode means all optimizations off (`-O0`), debugging
code inserted (assertions, runtime checks, etc.), and 
debugging symbols will be generated.

The binary produced will be noticeably slower, but suitable for
debugging in lldb or gdb.

`make debug` : builds o.se in debug mode

`make all-modules-debug` : builds all modules in debug mode

`make all-debug` : builds o.se and all modules in debug mode

### Clean

Clean removes built products and generated files. `make clean`
generally operates on the directory that it was called in, but
will not clean the directories that contain dependencies. 
For example, running `make clean` in the o.se.oscbn folder
will not cause the antlr4 runtime to be cleaned.

`make clean` : cleans the top level o.se folder

`make all-modules-clean` : cleans the folders for all modules

`make all-clean` : cleans all folders

## Build Options

The following environment variables (shown with their defaults) can
be used to control the build process:

    CCOMPILER=clang
    CPPCOMPILER=clang++
    DEBUG_SYMBOLS=lldb
    EXTRA_CFLAGS=
    EXTRA_CPPFLAGS=
    
To build everything in debug mode using gcc instead of clang, for
example, you could invoke `make` like this:

```
CCOMPILER=gcc CPPCOMPILER=g++ DEBUG_SYMBOLS=gdb make all-modules-debug
CCOMPILER=gcc DEBUG_SYMBOLS=gdb make debug
```
