# Vulkan Mandelbrot Renderer

A real-time Mandelbrot set renderer written in C++23 with the Vulkan API and Win32 with
interactive navigation and offline rendering to PNG using compute shaders.


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
