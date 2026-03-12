# Terminal Core

Terminal Core is the first construction pass for a retro-styled assembly puzzle game built in C++. The current scaffold includes:

- a clean multiplatform `CMake` project
- an `SDL2` + `OpenGL` application shell
- a custom retro workstation UI with a code editor, command console, and inspector panes
- a sandboxed 16-bit VM with a small instruction set
- external level files loaded from `assets/levels`
- toggleable CRT-style presentation controls

This is an early foundation build intended to validate the project structure, VM shape, and overall feel before deeper systems like the level editor, richer devices, and Steamworks integration are added.

## Current controls

- `Tab`: switch focus between the code editor and command prompt
- `F5`: assemble and run
- `F10`: single-step
- `F6`: reset the machine
- `Esc`: quit
- mouse: focus panes and drag the divider between the editor and inspector

## In-game commands

- `help`
- `levels`
- `load <id>`
- `assemble`
- `run`
- `step`
- `reset`
- `theme amber|green`
- `crt on|off`
- `scanlines on|off`
- `quit`

## Dependencies

- CMake 3.20+
- a C++20 compiler
  - Visual Studio 2022 on Windows is recommended
  - GCC 11+ or Clang 14+ on Linux should work well
- OpenGL development files
- SDL2

The `CMakeLists.txt` first tries `find_package(SDL2)` and falls back to fetching SDL2 from GitHub with `FetchContent` if it is not already installed.

## Build on Windows

From the repository root:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run:

```powershell
./build/bin/Release/terminal_core.exe
```

Depending on your generator, the executable may also appear under `build/bin/terminal_core.exe`.

## Build on Linux

Install the core packages first if you do not already have them:

```bash
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
```

Then build:

```bash
cmake -S . -B build
cmake --build build -j
./build/bin/terminal_core
```

## Project layout

- `src/app`: application loop and level loading
- `src/render`: custom renderer and retro theme handling
- `src/ui`: text editor state and input helpers
- `src/vm`: assembler and virtual machine
- `assets/levels`: sample puzzle definitions
- `sdk`: Steamworks SDK drop kept separate for later integration

## Notes

- The VM is intentionally small and deterministic so it can become the single source of truth for gameplay, scoring, and editor playtests.
- Steamworks is not yet wired into the runtime. The project is structured so it can be added behind a platform adapter later.
