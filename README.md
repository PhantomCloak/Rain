# WebEngine

Next-generation web engine. Written in C++. Uses the WebGPU rendering API, with native support for Windows, Linux, and macOS, as well as all WebGPU-compatible browsers including Safari (iOS) and Chrome (Android)

[View on web browser &rarr;](https://phantomcloak.me)

### Demos

## Tech Stack

- [Flecs](https://github.com/SanderMertens/flecs) - High-performance Entity Component System (ECS)
- [Jolt Physics](https://github.com/jrouwe/JoltPhysics) - Physics engine for collision detection and rigid body simulation
- [Ozz Animation](https://github.com/guillaumeblanc/ozz-animation) - Skeletal animation and skinning
- [Assimp](https://github.com/assimp/assimp) - Asset importer supporting 40+ file formats
- [ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI for debug tooling
- [Emscripten](https://github.com/emscripten-core/emscripten) - C++ to WebAssembly compiler toolchain

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

### Building the engine

### Windows Dependencies

- Visual Studio 2022 or above
- CMake
- Git

### Web Build Dependencies

- Emscriptten SDK 4.0.10

### OSX Dependencies

- Homebrew
- Git

1. Run setup_deps.ps1 in the root folder to install required dependencies

- Web: `cmake --preset emscripten`
- Windows: `cmake --preset windows-vs2022-debug`
- Linux: `cmake --preset linux-debug`
- MacOS: `cmake --preset osx-debug`
