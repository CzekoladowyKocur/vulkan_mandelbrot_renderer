# Vulkan Mandelbrot Renderer

[![CI](https://github.com/martin-swe-fs/vulkan_mandelbrot_renderer/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/martin-swe-fs/vulkan_mandelbrot_renderer/actions/workflows/ci.yml)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS-blue)
![Compilers](https://img.shields.io/badge/compilers-MSVC%20%7C%20clang--cl%20%7C%20AppleClang-blue)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3%2B-red)

A real-time Mandelbrot set renderer written in C++23 with the Vulkan API and Win32 with
interactive navigation and offline rendering to PNG using compute shaders.

https://github.com/user-attachments/assets/6ff97a7c-fe98-41ef-9f14-5b64f2bbf689

## Features

- Real-time fractal rendering with a graphics pipeline
- Offline rendering path using compute shaders, written out to PNG
- Interactive pan, zoom and iteration control
- Two compiler toolchains supported and enforced in CI: MSVC and clang-cl, both warning-clean with warnings-as-errors
- Static analysis (clang-tidy) and formatting (clang-format) enforced via git hooks and CI
- CMake presets for all supported toolchains, optional AddressSanitizer preset

## Building

Prerequisites (Windows):
- Visual Studio 2026 with clang-cl
- CMake 3.30+
- Vulkan SDK 1.3+

```
cmake --preset msvc
cmake --build --preset msvc-debug
```

Available configure presets: `msvc`, `clang` (clang-cl), `msvc-asan` (AddressSanitizer) and
`ninja` (compile database for clang-tidy). Shaders are
compiled to SPIR-V automatically as part of the build. Dependencies are
fetched by CMake.

## Running

```
build/msvc/Debug/vulkan_mandelbrot_renderer.exe
```

Run from the repository root so the relative `assets/` paths resolve. Pass `--compute` to render
offline to `mandelbrot.png` instead of opening the interactive window.

### Controls

| Key | Action |
|-----|--------|
| W / S | Move up / down |
| A / D | Move left / right |
| Z / X | Zoom in / out |
| UP / DOWN | Increase / decrease iterations |
| C / V | Tighten / widen color bands |
| LEFT / RIGHT | Shift the color palette |
| SHIFT | Move faster |

## Development setup

Git for Windows is required.

```sh
git config core.hooksPath hooks
```

clang-tidy needs a compile database. Generate it with the ninja preset from an
**x64 Native Tools Command Prompt**:

```
cmake --preset ninja
```

## Offline compute rendering

![OfflineRendering](showcase/ComputeMandelbrot.png)
