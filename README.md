# WebEngine

Next-generation web engine. Written in C++. Uses the WebGPU rendering API, with native support for Windows, Linux, and macOS, as well as all WebGPU-compatible browsers including Safari (iOS) and Chrome (Android)

[![Web](https://github.com/PhantomCloak/WebEngine/actions/workflows/pages.yml/badge.svg)](https://github.com/PhantomCloak/WebEngine/actions/workflows/pages.yml)
[![Windows](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-windows.yml/badge.svg)](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-windows.yml) [![Linux](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-linux.yml/badge.svg)](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-linux.yml) [![MacOS](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-macos.yml/badge.svg)](https://github.com/PhantomCloak/WebEngine/actions/workflows/build-macos.yml)

[View on web browser &rarr;](https://phantomcloak.me)

### Demos

## Tech Stack

- [Flecs](https://github.com/SanderMertens/flecs) - High-performance Entity Component System (ECS)
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics) - Physics engine for collision detection and rigid body simulation
- [Ozz Animation](https://github.com/guillaumeblanc/ozz-animation) - Skeletal animation and skinning
- [Assimp](https://github.com/assimp/assimp) - Asset importer supporting 40+ file formats
- [ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI for debug tooling
- [Emscripten](https://github.com/emscripten-core/emscripten) - C++ to WebAssembly compiler toolchain
- [Tracy](https://github.com/wolfpld/tracy) - A real time, nanosecond resolution, remote telemetry, hybrid frame and sampling profiler
- [CodeChecker](https://github.com/Ericsson/codechecker) - Static analysis tool built on LLVM/Clang Static Analyzer toolchain

## Rendering

Built on **WebGPU**, running natively via [Dawn](https://dawn.googlesource.com/dawn) (Vulkan, Metal, DX12) and in all modern browsers via Emscripten/WebAssembly

- **Physically based forward rendering** with Cook-Torrance BRDF — GGX normal distribution, Smith geometry, and Schlick Fresnel
- **Image-Based Lighting (IBL)** — irradiance and pre-filtered specular maps computed via compute shaders
- **Instanced draw calls** batched by mesh/material; transforms stored as a compact 3×vec4 and reconstructed on the GPU
- **Shader reflection** — parses WGSL and generates reflection info for WGSL shaders
- **Dynamic bindings** via `BindingManager` — binding layouts are dynamically generated from reflection info by name, with per-resource invalidation and validation
- **Cascaded shadow maps** with per-frame recomputed splits based on the camera frustum
- **GPU mipmap generation** via compute shaders at load time for 2D textures and cubemaps
- **GPU skeletal animation** — bone matrices in a storage buffer, 4-weight per-vertex blending in the vertex shader; animation runtime via Ozz-Animation
- **Dynamic material uniforms** — scalar and vector properties set by name, offsets resolved via shader reflection and written directly into the GPU uniform buffer
- **Modular render pass** — each pass independently declares its framebuffer, bind groups, and pipeline, with live shader hot-reload
- **Draco & KTX2 Support** — supports draco compressed vertex format and KTX supercompressed textures

## Building the engine
For Linux check `./setup_deps.sh` and windows `./setup_deps.ps1`.

### Dependencies
- MSVC or Clang, GCC
- CMake
- Git
- Python
- Emscriptten SDK 4.0.10

### Building
- Web: `cmake --preset emscripten && cmake --build build/emscripten-debug`
- Windows: `cmake --preset windows && cmake --build build/windows --config Release`
- Linux: `cmake --preset linux-debug && cmake --build build/linux-debug`
- MacOS: `cmake --preset osx-debug && cmake --build build/osx-debug`

