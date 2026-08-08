# Vita SDL2 starter

One small SDL2 codebase, with Docker-based Linux and PS Vita builds. Linux
compilation occurs in a pinned Fedora 42 container; Vita compilation and VPK
packaging use the maintained `vitasdk/vitasdk:latest` image. Neither toolchain
is installed on the host. `Dockerfile` exposes the named `linux` and `vita`
targets; the Makefile selects them for you.

## Prerequisites

- Docker (or a compatible Docker CLI/daemon) for every build.
- To run the Linux executable on Fedora, only the SDL2 runtime is required:
  `sudo dnf install SDL2`. Graphics drivers and a desktop session are also
  required, as for any SDL2 desktop game.

## Commands

```sh
make linux       # Docker build; executable: build/linux/vita_sdl2
make run         # Docker build, then launch the executable on the host
make vita        # builds Docker image, then build/vita/vita_sdl2.vpk
make clean
```

Install `build/vita/vita_sdl2.vpk` with VitaShell or another homebrew installer.
The sample exits with Escape on Linux and the Back button on Vita.

## Before shipping

Set `VITA_APP_NAME`, `VITA_TITLEID`, and `VITA_VERSION` in `CMakeLists.txt`.
Choose a unique nine-character uppercase/digit title ID. Add game data to the
VPK by passing `FILE source/path destination/path` pairs to `vita_create_vpk`.

## Project layout

```
src/main.cpp      Shared C++ SDL2 game entry point
CMakeLists.txt    Target-aware build and Vita packaging configuration
Dockerfile        Multi-stage Linux and official VitaSDK build environments
Makefile          Docker build and host-run commands
```
