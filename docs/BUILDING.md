# Building SDL3_gfx

SDL3_gfx is built with CMake (3.21 or newer) against an installed SDL3 (3.2.0 or newer).
The repository ships helper scripts that build a pinned SDL3 release locally, plus CMake presets
for Windows (MSVC + Ninja) and Linux; Emscripten builds the static library.

- [Directory layout](#directory-layout)
- [Windows](#windows)
- [Linux](#linux)
- [VS Code](#vs-code)
- [Tests](#tests)
- [Fuzzing](#fuzzing)
- [Benchmarks](#benchmarks)
- [Code style and static analysis](#code-style-and-static-analysis)
- [Build options](#build-options)
- [Emscripten](#emscripten)
- [Using an installed SDL3_gfx](#using-an-installed-sdl3_gfx)

## Directory layout

The scripts and presets assume this layout (the folder that contains the repository is called
`<root>`; any name works):

```
<root>\
  SDL3_gfx\            this repository
  external\SDL3\       SDL3 source (pinned tag, created by scripts/setup-sdl-*)
  build\SDL3\          SDL3 build tree
  install\sdl3\        SDL3 install prefix; the presets use it as CMAKE_PREFIX_PATH and install prefix
```

A different SDL3 location can be used by passing `-DCMAKE_PREFIX_PATH=<prefix>` or by creating a
`CMakeUserPresets.json` (git-ignored) that inherits from the shipped presets.

## Windows

Requirements: Windows 10/11 x64, [Visual Studio 2022 or later](https://visualstudio.microsoft.com/)
with the *Desktop development with C++* workload, Git, CMake ≥ 3.21, Python 3.

1. Install the remaining tools (Ninja, LLVM clang-format/clang-tidy, Doxygen, cppcheck; optional
   Graphviz and GitHub CLI) system-wide via winget. Open a new terminal afterwards.

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\setup-tools-windows.ps1
   ```

2. Build and install SDL3 (default: `release-3.4.20`) into `<root>\install\sdl3`:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\setup-sdl-windows.ps1
   ```

3. Configure, build and test. Command-line builds need the MSVC environment, which
   `scripts\devshell.ps1` sets up in the current PowerShell session:

   ```powershell
   . .\scripts\devshell.ps1
   cmake --preset win-debug
   cmake --build --preset win-debug
   ctest --preset win-debug
   cmake --install build\win-debug        # installs into <root>\install\sdl3
   ```

   Executables, `SDL3_gfx.dll`, `SDL3.dll` and the sample images end up in `build\<preset>\bin`.

Available presets: `win-debug`, `win-release` (RelWithDebInfo), `win-analyze` (MSVC `/analyze`),
`win-fuzz` (libFuzzer + AddressSanitizer).

## Linux

Requirements: a recent Debian/Ubuntu or Fedora. The tool script installs compilers, CMake, Ninja,
analysis tools and the SDL build dependencies (X11, Wayland, audio, ...).

```bash
./scripts/setup-tools-linux.sh
./scripts/setup-sdl-linux.sh            # builds SDL3 into <root>/install/sdl3
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

A distribution SDL3 package (3.2.0 or newer, e.g. `libsdl3-dev`, `SDL3-devel`) works as well; the presets fall
back to the system SDL3 when `<root>/install/sdl3` does not exist.

Available presets: `linux-debug`, `linux-release`, `linux-fuzz` (clang libFuzzer + ASan/UBSan).

## VS Code

Open the repository folder (or the multi-root `.code-workspace` of the modernization workspace).
Recommended extensions are listed in `.vscode/extensions.json` (CMake Tools, C/C++).

- CMake Tools reads `CMakePresets.json`; pick a configure preset in the status bar. On Windows,
  CMake Tools sets up the MSVC environment automatically (no devshell needed).
- *Build*, *Test* (CTest) and *Debug* are available from the status bar / CMake side bar.
- `.vscode/launch.json` debugs the selected launch target with the working directory set to the
  executable folder, where the sample images are copied.

## Tests

`ctest --preset <preset>` runs the automated tests (the presets skip the `timing` label):

| CTest name | Labels | Content |
|---|---|---|
| `Primitives` | golden, unit | Golden-image test per primitive family (edge cases: reversed/degenerate coordinates, clipping, alpha blending, text), `...Color` vs `...RGBA` equivalence, parameter validation |
| `Rotozoom` | golden, unit | Size calculations, exact zoom/flip/90° rotation/shrink mappings, 8/24/32-bit handling, golden image |
| `ImageFilter` | unit | Every byte filter against its documented formula (lengths 0..67, unaligned buffers, canary bytes) |
| `Framerate` | unit | Parameter validation and frame counting |
| `PrimitivesGPU` | gpu | Every hardware render driver (hidden window, target texture): anti-aliased pixels as batched geometry are bit-identical to individual points, and all primitives match the software renderer within ±2 per channel. Skipped without a hardware renderer |
| `FramerateTiming` | timing | Delay accuracy (machine dependent; run with `ctest --test-dir build/<preset> -L timing`) |
| `package.find_package` | package | Installs into a scratch prefix and builds/runs a `find_package(SDL3_gfx)` consumer |
| `parity.Primitives`, `parity.Rotozoom`, `parity.api` | parity | Only with an SDL2_gfx checkout (`SDL3GFX_PARITY_SDL2GFX_DIR`, default `../SDL2_gfx`): the scenes must match the SDL2_gfx golden images, and `scripts/check_api_parity.py` compares the public headers |

The suites use SDL3's `SDL_test` harness in one executable, `gfxtests` (`gfxtests --help`). Rendering
is headless: a software renderer draws into an `SDL_Surface` (64x64 canvas) that is compared with
`test/golden/sdl3/*.bmp` without tolerance. These images are identical to the SDL2_gfx golden images:
SDL3_gfx draws the same pixels. On a mismatch the actual image is written to
`build/<preset>/test/failed/`. Only `PrimitivesGPU` uses the real video driver (`gfxtests --real-video`).

The hint `SDL3_GFX_AA_GEOMETRY` (`1`/`0`) forces or disables drawing anti-aliased pixels as
`SDL_RenderGeometry` quads; by default hardware renderers use quads and the software renderer points.

Golden images capture the behavior of the code when they were generated. After an intentional change,
regenerate them and review the image diff before committing (reference the finding in
`docs/CODE_REVIEW.md`). Drawing changes belong in SDL2_gfx as well, so that the parity tests keep passing:

```powershell
cmake --build --preset win-debug --target gfxtests_generate_golden
```

Test cases that document a known bug are present but disabled (`TEST_DISABLED`) with the finding ID
in their description; they are enabled together with the fix.

## Fuzzing

`fuzz/` contains [libFuzzer](https://llvm.org/docs/LibFuzzer.html) targets for the primitives
(`fuzz_primitives`: a command stream with arbitrary coordinates, radii, angles, vertex arrays, text),
the rotozoomer (`fuzz_rotozoom`), the image filters (`fuzz_imagefilter`, exactly sized buffers) and
custom fonts (`fuzz_font`).

- Every normal build also builds `fuzz_<target>_replay` programs; CTest replays the committed corpus
  `fuzz/corpus/<target>/` (label `fuzz-regression`).
- The `win-fuzz` preset (MSVC, needs the *C++ AddressSanitizer* VS component) and `linux-fuzz`
  (clang, ASan + UBSan) build the libFuzzer executables and instrument everything with
  AddressSanitizer, so `ctest --preset win-fuzz` also runs all test suites under ASan.

```powershell
. .\scripts\devshell.ps1
cmake --preset win-fuzz
cmake --build --preset win-fuzz
ctest --preset win-fuzz
# fuzz for 10 minutes; new inputs go to the scratch corpus, crashes to crash-<hash>
mkdir build\win-fuzz\work
build\win-fuzz\bin\fuzz_primitives.exe build\win-fuzz\work fuzz\corpus\primitives -max_total_time=600
# reproduce a crash with the replay program (any build)
build\win-debug\bin\fuzz_primitives_replay.exe crash-<hash>
# add new coverage to the committed corpus (minimized)
build\win-fuzz\bin\fuzz_primitives.exe -merge=1 fuzz\corpus\primitives build\win-fuzz\work
```

`fuzz/make_seeds.py` regenerates the seed inputs. Inputs that still crash or time out are kept in
`fuzz/regressions/` (not replayed) until the corresponding finding is fixed; then they move to the corpus.

## Benchmarks

`gfxbench` (built with the demos, `SDL3GFX_BENCH`) measures operations per second for every primitive
(opaque and blended), the rotozoomer (256x256 surface) and MB/s for the image filters. Use a Release
build and an otherwise idle machine; results are tracked in [PERFORMANCE.md](PERFORMANCE.md).

```powershell
build\win-release\bin\gfxbench.exe --csv bench-software.csv
build\win-release\bin\gfxbench.exe --renderer direct3d11 --csv bench-d3d11.csv
build\win-release\bin\gfxbench.exe --filter circle --time 1
```

## Code style and static analysis

- Formatting is defined in `.clang-format` (tabs, Linux brace style, existing line breaks kept).
  Run `scripts/format.ps1` (Windows) or `scripts/format.sh` (Linux) to format all sources, or add
  `-Check` / `--check` to verify only. Files in `.clang-format-ignore` are skipped.
- `scripts/analyze.ps1` runs MSVC `/analyze` (preset `win-analyze`), clang-tidy (`.clang-tidy`) and
  cppcheck on the library sources and writes reports to `build/analysis/`; `scripts/analyze.sh` runs
  clang-tidy and cppcheck on Linux. Use `-Strict` / `--strict` to fail on any finding.

## Build options

| Option | Default | Description |
|---|---|---|
| `SDL3GFX_SHARED` | `ON` (Emscripten: `OFF`) | Build the shared library (`SDL3_gfx::SDL3_gfx-shared`; `SDL3_gfx::SDL3_gfx` refers to it if built) |
| `SDL3GFX_STATIC` | `OFF` | Build the static library (`SDL3_gfx::SDL3_gfx-static`) |
| `SDL3GFX_DEMOS` | top-level, not Emscripten | Build the interactive demo programs in `demo/` |
| `SDL3GFX_TESTS` | top-level, not Emscripten | Build the CTest suites in `test/` |
| `SDL3GFX_INSTALL` | top-level | Generate install rules |
| `SDL3GFX_WERROR` | `OFF` | Treat warnings as errors |
| `SDL3GFX_ANALYZE` | `OFF` | Run the MSVC code analyzer on the library sources |
| `SDL3GFX_FUZZ` | `OFF` | Build libFuzzer targets; instrument everything with AddressSanitizer |
| `SDL3GFX_BENCH` | top-level, not Emscripten | Build the `gfxbench` benchmark program |
| `SDL3GFX_DOCS` | top-level | Add the `docs` target (Doxygen HTML, needs Doxygen; Graphviz optional) |
| `SDL3GFX_LEGACY_COLOR_BYTE_ORDER` | `OFF` | `...Color` functions use the color bytes in memory order (behavior of SDL3_gfx 1.0.x and SDL2_gfx up to 1.0.4) instead of `0xRRGGBBAA` |

## API documentation

`cmake --build --preset win-release --target docs` generates the HTML reference from the Doxygen comments in
`build/<preset>/docs/html/index.html` (Doxygen is found in `C:\Program Files\doxygen` or on `PATH`). Doxygen
warnings, e.g. an undocumented public function or parameter, fail the target. The HTML is not committed.

## Emscripten

Emscripten builds only the static library (no tests, demos or benchmark). SDL3 comes from an SDL3 build for
Emscripten in `CMAKE_PREFIX_PATH` or, if there is none, from the SDL3 port of Emscripten (`--use-port=sdl3`, which
is also added to the link options of programs using `SDL3_gfx::SDL3_gfx`):

```bash
emcmake cmake -S . -B build/emscripten -DCMAKE_BUILD_TYPE=Release
cmake --build build/emscripten
cmake --install build/emscripten --prefix /path/to/install
```

This configuration is not covered by the automated tests.

## Using an installed SDL3_gfx

Installed files: headers in `include/SDL3_gfx/`, the library (`SDL3_gfx.dll`/`.lib`, `libSDL3_gfx.so.2`,
`libSDL3_gfx.a`), `lib/cmake/SDL3_gfx/` (CMake package) and `lib/pkgconfig/sdl3-gfx.pc`.

CMake:

```cmake
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_gfx REQUIRED CONFIG)
target_link_libraries(app PRIVATE SDL3_gfx::SDL3_gfx SDL3::SDL3)
```

`SDL3_gfx::SDL3_gfx` is the shared library if it was installed, otherwise the static library; use
`SDL3_gfx::SDL3_gfx-shared` or `SDL3_gfx::SDL3_gfx-static` to choose.

pkg-config:

```bash
cc app.c $(pkg-config --cflags --libs sdl3-gfx) -o app
```

Headers are included like the SDL3 headers: `#include <SDL3_gfx/SDL3_gfxPrimitives.h>` (the `SDL3_gfx` folder is
also on the include path, so `#include "SDL3_gfxPrimitives.h"` works as well).