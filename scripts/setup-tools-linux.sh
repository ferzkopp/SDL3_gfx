#!/usr/bin/env bash
# Install the Linux packages needed to build SDL (1.2/2/3) and this repository.
# Debian/Ubuntu (apt) and Fedora (dnf) are supported. Run as a user with sudo rights.
#
#   ./scripts/setup-tools-linux.sh            # install
#   ./scripts/setup-tools-linux.sh --check    # only report tool status
set -euo pipefail

check_only=0
[[ "${1:-}" == "--check" ]] && check_only=1

if [[ $check_only -eq 0 ]]; then
    if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential git cmake ninja-build pkg-config python3 gdb \
            clang clang-format clang-tidy cppcheck doxygen graphviz \
            autoconf automake libtool nasm \
            libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
            libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
            libegl1-mesa-dev libgles2-mesa-dev libgl1-mesa-dev libdrm-dev libgbm-dev \
            libasound2-dev libpulse-dev libpipewire-0.3-dev libjack-dev libsndio-dev \
            libudev-dev libdbus-1-dev libibus-1.0-dev libusb-1.0-0-dev
    elif command -v dnf >/dev/null 2>&1; then
        sudo dnf install -y \
            gcc gcc-c++ make git cmake ninja-build pkgconf-pkg-config python3 gdb \
            clang clang-tools-extra cppcheck doxygen graphviz \
            autoconf automake libtool nasm \
            libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXfixes-devel libXi-devel \
            libXScrnSaver-devel libXtst-devel libxkbcommon-devel wayland-devel wayland-protocols-devel \
            libdecor-devel mesa-libEGL-devel mesa-libGLES-devel mesa-libGL-devel libdrm-devel mesa-libgbm-devel \
            alsa-lib-devel pulseaudio-libs-devel pipewire-devel jack-audio-connection-kit-devel \
            systemd-devel dbus-devel ibus-devel libusb1-devel
    else
        echo "Unsupported distribution: install the packages listed in this script manually." >&2
        exit 1
    fi
fi

missing=0
for t in git cmake ninja pkg-config python3 cc clang clang-format clang-tidy cppcheck doxygen gdb; do
    if p=$(command -v "$t"); then printf '  %-13s %s\n' "$t" "$p"
    else printf '  %-13s MISSING\n' "$t"; missing=$((missing + 1)); fi
done
[[ $missing -eq 0 ]] || { echo "$missing tool(s) missing" >&2; exit 1; }
