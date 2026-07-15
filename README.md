# Vulkan Mandelbrot Renderer

[![CI](https://github.com/CzekoladowyKocur/vulkan_mandelbrot_renderer/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/CzekoladowyKocur/vulkan_mandelbrot_renderer/actions/workflows/ci.yml)
![Status](https://img.shields.io/badge/status-active-brightgreen)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS-blue)
![Compilers](https://img.shields.io/badge/compilers-MSVC%20%7C%20clang--cl%20%7C%20AppleClang-blue)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue)
![Vulkan](https://img.shields.io/badge/Vulkan-1.3%2B-red)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

> [!NOTE]
> Modernization of my 2022 Windows renderer. Now cross-platform with macOS support and rewritten in C++23.

https://github.com/user-attachments/assets/8a15cdd9-bd0f-465a-9f19-635c799aa6e9

*A real-time GPU zoom into the Mandelbrot. ~800 iterations per pixel with smooth coloring.*

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

Prerequisites (macOS):
- Xcode command line tools and Ninja
- CMake 3.30+
- Vulkan SDK 1.3+ with MoltenVK

```
cmake --preset macos
cmake --build --preset macos-debug
```

Available configure presets: `msvc`, `clang` (clang-cl), `msvc-asan` (AddressSanitizer),
`macos` (Ninja, AppleClang) and `ninja` (compile database for clang-tidy). Shaders are
compiled to SPIR-V automatically as part of the build. Dependencies are
fetched by CMake.

## Running

```
cd build/msvc  && Debug/vulkan_mandelbrot_renderer.exe   # Windows
cd build/macos && Debug/vulkan_mandelbrot_renderer       # macOS
```

Run from the build directory so the relative `assets/` paths resolve. Pass `--compute` to render
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

```sh
git config core.hooksPath hooks
```

clang-tidy requires a compile database. On Windows, generate it with the ninja preset:

```
cmake --preset ninja
```

> [!IMPORTANT]
> On Windows the ninja preset must be configured from an **x64 Native Tools Command Prompt**
> (requires Git for Windows), otherwise the compile database will point at the wrong toolchain.

On macOS the `macos` preset already exports one, clang-tidy and clang-format come with
`brew install llvm`.

## Offline compute rendering

![OfflineRendering](showcase/ComputeMandelbrot.png)

*Rendered offline with the compute pipeline at 10,000 iterations per pixel.*

## Roadmap

- **Infinite zoom**: the current renderer is limited by floating-point precision.
    Arbitrarily deep zoom is achievable with perturbation theory.
- **Linux window backend**: Wayland and/or X11 support on top of the existing
    platform abstraction.
