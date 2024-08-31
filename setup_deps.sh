#!/bin/sh

sudo apt install cmake clang git # C++
sudo apt-get install xcb libxcb-xkb-dev x11-xkb-utils libx11-xcb-dev libxkbcommon-x11-dev xorg-dev

#fedora_version=$(grep -oP '(?<=^VERSION_ID=)\d+' /etc/os-release)
#if [ "$fedora_version" -ge 40 ]; then
#	sudo dnf install libxcrypt-compat
#fi

git submodule update --init --depth 1

bash "vendor/PhysX/physx/generate_projects.sh"


