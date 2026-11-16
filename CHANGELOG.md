# Changelog

All notable changes to SDL3_gfx. Versions 1.0.x are the first SDL3 port by Sulaiman Abdul-Khabir
(<https://github.com/sabdul-khabir/SDL3_gfx>, tag `v1.0.1` in this repository). Version 2.0 is the
SDL3 version of SDL2_gfx 1.1 and replaces the 1.0.x API; see [docs/MIGRATION.md](docs/MIGRATION.md).

## 2.0.0 (unreleased)

- **API:** the SDL2_gfx API (decision D8 of the modernization): `Sint16` coordinates and radii instead of
  `float`, `int` results (0 = success, -1 = error) instead of `bool`, `SDL_imageFilterMultNor` restored,
  `SDL_imageFilterShiftRightUint`/`ShiftLeftUint` without the `bpp` parameter. `SDL_framerateDelay` returns
  `Uint32`; `FPSmanager` keeps 64-bit tick fields. Programs written for SDL2_gfx compile unchanged apart from
  includes and link targets.
- **Image filters:** the results are those of SDL2_gfx again. The 1.0.x changes of `Mult`, `MultDivby2`,
  `MultDivby4`, `MultByByte`, `ShiftRightAndMultByByte`, `Div`, `Mean` and `NormalizeLinear` (normalized float
  math, PR #13) are reverted: they changed documented results, `Mult` used the first source twice, `Div` returned
  only 0 or 255 and the `...Uint` shifts combined the bytes incorrectly. `SDL_imageFilterMultUnbound` (same as
  `MultNor`) and `SDL_imageFilterMultInv` (`255 - (255 - S1) * (255 - S2) / 255`, now rounded integer math) are kept.
- **Colors:** `...Color` functions decode `0xRRGGBBAA` as documented; 1.0.x used the bytes in memory order
  (`0xAABBGGRR` on little-endian machines). The CMake option `SDL3GFX_LEGACY_COLOR_BYTE_ORDER=ON` restores it.
- All fixes of SDL2_gfx 1.1 (see its changelog and [docs/CODE_REVIEW.md](docs/CODE_REVIEW.md)): text with several
  renderers (FONT-001), oversized fonts (FONT-002), rotozoom with different x/y zoom (ROTO-001), 8-bit zoom with
  flips (ROTO-002), `shrinkSurface` factor checks (ROTO-003), NaN/infinite/huge rotozoom parameters (ROTO-004),
  rounded boxes (PRIM-004, PRIM-012), full-circle arcs (PRIM-005), thread-safe and 32-bit correct polygon fills
  (PRIM-002, PRIM-006, PRIM-007), `texturedPolygon` result and no `SDL_RenderPresent` (PRIM-001, PRIM-013),
  zero-length thick lines (PRIM-010), each pixel blended once (PRIM-016), NULL framerate manager (FRM-001),
  long-running framerate schedule (FRM-002), portable C image filters (IMGF-001 to IMGF-006).
- `rotateSurface90Degrees` keeps the palette of 8-bit surfaces (ROTO-005).
- Large shapes: ellipses and anti-aliased ellipses with radii above about 1000 have the correct shape (32-bit
  overflow, PRIM-019); anti-aliased lines spanning 32768 pixels or more have the correct slope (PRIM-018); thick
  lines and bezier curves beyond the coordinate range are clamped instead of wrapping around (PRIM-020).
- Rotozoomer: 64-bit source positions, correct for small zoom factors and sources wider than 32767 pixels
  (ROTO-008); no division by zero for a smooth zoom to one pixel (ROTO-007); `shrinkSurface` with a factor larger
  than the source returns its empty surface without touching NULL pixels (ROTO-009); no undefined shifts (ROTO-006).
- Performance: draw call batching (PERF-002) and visible-rows-only polygon fills (PERF-001); see
  [docs/PERFORMANCE.md](docs/PERFORMANCE.md).
- Only the functions declared in the headers are exported. SONAME `libSDL3_gfx.so.2` (1.0.x: `.so.1`).
- Build: CMake with presets, CMake package `SDL3_gfx` (targets `SDL3_gfx::SDL3_gfx`, `SDL3_gfx::SDL3_gfx-shared`,
  `SDL3_gfx::SDL3_gfx-static`), pkg-config `sdl3-gfx.pc`, headers in `include/SDL3_gfx`; Emscripten builds the
  static library. Removed the Visual Studio solution and the committed HTML documentation (`docs` target).
- Tests: CTest suites with golden images (identical to those of SDL2_gfx), hardware renderer comparisons, fuzz
  targets, a package consumer test and `parity` tests against an SDL2_gfx checkout. Interactive programs moved
  to `demo/`, sample images to `test/data/`; benchmark in `bench/`.

## 1.0.x after 1.0.1 (unreleased, 2025-05 to 2026-02)

- Pkg-config file `sdl3-gfx.pc` (thanks hdejin, #7)
- Removed the MMX filter code and fixed the `...Uint` filters (#12)
- Modern `find_package` in CMake, version from `PROJECT_VERSION` (#16)
- Image filter changes: normalized `Mult`, `MultUnbound` (renamed `MultNor`) and new `MultInv`, `bpp` parameter
  for the `...Uint` shifts (#13)
- Image filters and primitives return `bool`; primitives use `float` coordinates (#17, #18)
- Special case for circles/ellipses with radii between 0 and 1 (#20)
- Visual Studio Win32 target uses SDL3

## 1.0.1 - 2025-01-28

- Fixed bug and improvements (thanks DuilioPerez)
- Fixed testimagefilter log formatting (SDL_Log automatically inserts a newline in SDL3)
- Added more screenshots
- CMake improvements and Emscripten support (#4, #5)

## 1.0.0 - 2025-01-16

- Added CMake files to replace autotools; removed the outdated xcode.zip
- Fixed some logging statements
- Renamed SDL2* files to SDL3*; SDL header included as `<SDL3/SDL.h>`
- In SDL3_framerate.c, tick variables changed from Uint32 to Uint64 since SDL tick functions return 64-bit values
- Updated the MSVC project to include an x64 target
- SDL3_gfxPrimitives functions return bools instead of integers
- Renamed SDL2 functions to their SDL3 equivalents, e.g. SDL_FreeSurface() to SDL_DestroySurface()
- Palettes are separate from surfaces in SDL3: palettes are created for indexed surfaces in SDL3_rotozoom

## Earlier versions

SDL3_gfx started from SDL2_gfx 1.0.4; see the changelog of [SDL2_gfx](https://github.com/ferzkopp/SDL2_gfx).