#!/usr/bin/env bash
# Clone, build and install the SDL version used by this repository (Linux).
# Same layout and pinned versions as scripts/setup-sdl-windows.ps1:
#
#   <deps-root>/external/<name>   upstream clone (pinned)
#   <deps-root>/build/<name>      build tree
#   <deps-root>/install/sdl<N>    install prefix used by the CMake presets
#
# Usage: ./scripts/setup-sdl-linux.sh [--sdl 3|2|1-classic|1-compat] [--deps-root DIR]
#                                     [--ref TAG_OR_SHA] [--config RelWithDebInfo] [--clean]
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(dirname "$script_dir")
sdl="" deps_root="" ref="" config="RelWithDebInfo" clean=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --sdl) sdl="$2"; shift 2 ;;
        --deps-root) deps_root="$2"; shift 2 ;;
        --ref) ref="$2"; shift 2 ;;
        --config) config="$2"; shift 2 ;;
        --clean) clean=1; shift ;;
        -h|--help) sed -n '2,11p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

if [[ -z "$sdl" ]]; then
    if   [[ -f "$repo_root/SDL3_gfxPrimitives.h" ]]; then sdl=3
    elif [[ -f "$repo_root/SDL2_gfxPrimitives.h" ]]; then sdl=2
    elif [[ -f "$repo_root/SDL_gfxPrimitives.h"  ]]; then sdl=1-classic
    else echo "cannot detect SDL version; pass --sdl" >&2; exit 2; fi
fi
deps_root=$(realpath -m "${deps_root:-$(dirname "$repo_root")}")

case "$sdl" in
    3)         url=https://github.com/libsdl-org/SDL.git;          pin=release-3.4.20;  name=SDL3;         pfx=sdl3 ;;
    2)         url=https://github.com/libsdl-org/SDL.git;          pin=release-2.32.10; name=SDL2;         pfx=sdl2 ;;
    1-classic) url=https://github.com/libsdl-org/SDL-1.2.git;      pin=81490298a02fe6806c05ac7b5afde7d88fcc7bf9; name=SDL-1.2; pfx=sdl1 ;;
    1-compat)  url=https://github.com/libsdl-org/sdl12-compat.git; pin=release-1.2.78;  name=sdl12-compat; pfx=sdl1 ;;
    *) echo "invalid --sdl $sdl" >&2; exit 2 ;;
esac
ref="${ref:-$pin}"
src="$deps_root/external/$name"
bld="$deps_root/build/$name"
prefix="$deps_root/install/$pfx"
jobs=$(nproc 2>/dev/null || echo 4)

provider_file="$prefix/sdl-provider.txt"
if [[ -f "$provider_file" && "$(cut -d' ' -f1 "$provider_file")" != "$name" ]]; then
    echo "Removing $prefix (installed by a different SDL provider)"
    rm -rf "$prefix"
fi

if [[ ! -d "$src/.git" ]]; then
    mkdir -p "$src"
    git -C "$src" init --quiet
    git -C "$src" remote add origin "$url"
fi
target="$ref"
if ! git -C "$src" rev-parse --verify --quiet "$ref^{commit}" >/dev/null; then
    git -C "$src" fetch --quiet --depth 1 origin "$ref"
    target=FETCH_HEAD
fi
git -C "$src" -c advice.detachedHead=false checkout --quiet --force "$target"
echo "$src @ $(git -C "$src" rev-parse --short HEAD) ($ref)"

[[ $clean -eq 1 ]] && rm -rf "$bld"

cmake_sdl() {
    cmake -S "$src" -B "$bld" -G Ninja -DCMAKE_BUILD_TYPE="$config" -DCMAKE_INSTALL_PREFIX="$prefix" "$@"
    cmake --build "$bld"
    cmake --install "$bld"
}

case "$sdl" in
    3) cmake_sdl -DSDL_TEST_LIBRARY=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL_DOCS=OFF ;;
    2) cmake_sdl -DSDL_TEST=ON -DSDL_TESTS=OFF ;;
    1-compat)
        if [[ ! -f "$deps_root/install/sdl2/lib/cmake/SDL2/SDL2Config.cmake" ]]; then
            "$0" --sdl 2 --deps-root "$deps_root" --config "$config"
        fi
        cmake_sdl -DCMAKE_PREFIX_PATH="$deps_root/install/sdl2" -DSDL12TESTS=OFF -DSDL12DEVEL=ON
        ;;
    1-classic)
        mkdir -p "$bld"
        cflags="-O2 -g"; [[ "$config" == "Debug" ]] && cflags="-O0 -g"
        # Classic SDL 1.2 uses autotools; configure is committed in the repository.
        (cd "$bld" && CFLAGS="$cflags" "$src/configure" --prefix="$prefix" --disable-static --disable-nasm)
        make -C "$bld" -j"$jobs"
        make -C "$bld" install
        ;;
esac

echo "$name $ref" > "$provider_file"
echo "Done: $name ($ref) installed to $prefix"
