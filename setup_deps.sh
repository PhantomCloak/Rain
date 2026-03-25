#!/bin/sh

git submodule update --init --depth 1 vendor/dawn
git submodule update --init --recursive vendor/spdlog vendor/imgui vendor/assimp vendor/flecs vendor/tracy vendor/stb vendor/ImGuizmo vendor/JoltPhysics vendor/ozz-animation

if [ "$(uname -s)" = "Linux" ]; then
  sudo apt install cmake clang git
  sudo apt-get install xcb libxcb-xkb-dev x11-xkb-utils libx11-xcb-dev libxkbcommon-x11-dev xorg-dev
fi
