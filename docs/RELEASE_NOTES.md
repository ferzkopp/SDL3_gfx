# SDL3_gfx 2.0.0

SDL3_gfx 2.0 is the SDL3 version of SDL2_gfx 1.1: the same API, the same fixes and performance work, and the same
pixels (the automated tests use the SDL2_gfx golden images). Porting an SDL2_gfx program to SDL3 only changes the
includes and link targets on the SDL3_gfx side. Requires SDL 3.2.0 or newer.

## Incompatible with SDL3_gfx 1.0.x

Versions 1.0.0 and 1.0.1 (the first SDL3 port by Sulaiman Abdul-Khabir, thanks to him and the contributors) used
`float` coordinates and `bool` results. 2.0 returns to the SDL2_gfx API:

- results are `int`: **0 = success, -1 = error** (the opposite truth value of the 1.0.x `bool`)
- coordinates and radii are `Sint16`
- image filters compute the SDL2_gfx results again; `SDL_imageFilterMultNor` is back, `MultUnbound` and
  `MultInv` remain; the `...Uint` shifts have no `bpp` parameter
- `...Color` functions decode `0xRRGGBBAA` (`-DSDL3GFX_LEGACY_COLOR_BYTE_ORDER=ON` restores the 1.0.x byte order)
- the shared library is `libSDL3_gfx.so.2` (1.0.x: `.so.1`)

See [MIGRATION.md](https://github.com/ferzkopp/SDL3_gfx/blob/v2.0.0/docs/MIGRATION.md) for details, also for porting from SDL2_gfx.

## Highlights

- All fixes of SDL2_gfx 1.1 (text with several renderers, rotozoom, polygons, framerate, image filters, ...).
- Draw call batching: fast with hardware renderers, including SDL3's GPU renderer.
- CMake package `SDL3_gfx` (`SDL3_gfx::SDL3_gfx`, `-shared`, `-static`), pkg-config `sdl3-gfx`, headers in
  `include/SDL3_gfx`; Emscripten builds the static library.
- Tests: golden images, hardware renderer comparisons, fuzzing, and parity tests against SDL2_gfx.

## Known differences from SDL2

- With a render scale below 1, SDL3's software renderer drops lines and rectangles thinner than one pixel, so most
  primitives disappear there (SDL2 drew them one pixel wide). Hardware renderers are not affected.
- Anti-aliased lines and polygons are slower than SDL2_gfx on SDL2 (about 0.6-0.75x); simple primitives are up to
  2.5x faster with Direct3D 11. See [PERFORMANCE.md](https://github.com/ferzkopp/SDL3_gfx/blob/v2.0.0/docs/PERFORMANCE.md).

Full list of changes: [CHANGELOG.md](https://github.com/ferzkopp/SDL3_gfx/blob/v2.0.0/CHANGELOG.md). Review: [CODE_REVIEW.md](https://github.com/ferzkopp/SDL3_gfx/blob/v2.0.0/docs/CODE_REVIEW.md).