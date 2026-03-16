#!/bin/sh

# Init Dawn WITHOUT --recursive (its deps are fetched by DAWN_FETCH_DEPENDENCIES in CMake)
git submodule update --init --depth 1 vendor/dawn

# Init everything else WITH --recursive
git submodule update --init --recursive vendor/spdlog vendor/imgui vendor/assimp vendor/flecs vendor/tracy vendor/stb vendor/ImGuizmo vendor/JoltPhysics vendor/ozz-animation

# Platform-specific setup
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    echo "Generating Visual Studio solution..."
    cmake --preset windows-vs2022-debug
    ;;
  Linux*)
    sudo apt install cmake clang git
    sudo apt-get install xcb libxcb-xkb-dev x11-xkb-utils libx11-xcb-dev libxkbcommon-x11-dev xorg-dev
    cmake --preset linux-debug
    ;;
  Darwin*)
    cmake --preset osx-debug
    ;;
esac
