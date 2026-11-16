# Migrating to SDL3_gfx 2.x

SDL3_gfx 2.x has the API of SDL2_gfx 1.1 and draws the same pixels (the automated tests use the same golden
images). This page lists what changes when porting a program from SDL2_gfx and from the first SDL3 port,
SDL3_gfx 1.0.x.

## From SDL2_gfx

The SDL3_gfx part of the port is mechanical; most of the work is porting the program itself to SDL3 (see
SDL's [README-migration](https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md)).

| | SDL2_gfx | SDL3_gfx |
|---|---|---|
| Includes | `#include "SDL2_gfxPrimitives.h"` (headers in `include/SDL2`) | `#include <SDL3_gfx/SDL3_gfxPrimitives.h>` (headers in `include/SDL3_gfx`; that folder is also on the include path of the CMake targets and the pkg-config file, so `"SDL3_gfxPrimitives.h"` works too) |
| Other headers | `SDL2_rotozoom.h`, `SDL2_framerate.h`, `SDL2_imageFilter.h` | `SDL3_rotozoom.h`, `SDL3_framerate.h`, `SDL3_imageFilter.h` |
| CMake | `find_package(SDL2_gfx)`, `SDL2_gfx::SDL2_gfx` / `SDL2_gfx::SDL2_gfx-static` | `find_package(SDL3_gfx)`, `SDL3_gfx::SDL3_gfx` / `SDL3_gfx::SDL3_gfx-shared` / `SDL3_gfx::SDL3_gfx-static` |
| pkg-config | `SDL2_gfx` | `sdl3-gfx` |
| Library | `SDL2_gfx.dll`, `libSDL2_gfx-1.0.so.0` | `SDL3_gfx.dll`, `libSDL3_gfx.so.2` |
| Version macros | `SDL2_GFXPRIMITIVES_MAJOR/MINOR/MICRO` | `SDL3_GFXPRIMITIVES_MAJOR/MINOR/MICRO` (2.x) |
| Hint | `SDL2_GFX_AA_GEOMETRY` | `SDL3_GFX_AA_GEOMETRY` |
| Legacy color option | `SDL2GFX_LEGACY_COLOR_BYTE_ORDER` | `SDL3GFX_LEGACY_COLOR_BYTE_ORDER` |

Function names, parameters and results are unchanged; `scripts/check_api_parity.py` verifies this against the
SDL2_gfx headers. Things to keep in mind:

- **Results:** SDL3_gfx functions return 0 on success and -1 on error like SDL2_gfx, while SDL3's own functions
  return `true` on success. Code such as `if (boxRGBA(...) < 0)` stays correct; do not mix the two conventions.
- **`FPSmanager`:** `baseticks` and `lastticks` are `Uint64` (SDL3's `SDL_GetTicks` returns 64 bits).
  `SDL_framerateDelay` still returns the elapsed milliseconds as `Uint32`. Only programs that access the
  structure fields directly are affected.
- **Surfaces:** the rotozoomer works on SDL3 surfaces. Results have the pixel format of the source
  (other formats than 8 and 32 bits per pixel are converted to `SDL_PIXELFORMAT_RGBA32` first); 8-bit results
  get their own copy of the source palette.
- **Additions:** `SDL_imageFilterMultUnbound` (same as `SDL_imageFilterMultNor`) and `SDL_imageFilterMultInv`.
- **Render scale:** with a render scale below 1, SDL3's software renderer drops lines and rectangles thinner than
  one pixel; SDL2 drew them one pixel wide. Primitives drawn that way on the software renderer disappear
  (PRIM-017 in [CODE_REVIEW.md](CODE_REVIEW.md)). Hardware renderers are not affected.

## From SDL3_gfx 1.0.x

SDL3_gfx 1.0.0 and 1.0.1 (and the later unreleased changes of the first SDL3 port) changed the SDL2_gfx API.
Version 2.0 returns to the SDL2_gfx API, so code written for 1.0.x needs these changes:

- **Results are `int` again, with the opposite meaning of the `bool` results:** 0 means success and -1 means
  error. `if (!filledCircleRGBA(...))` (error handling for 1.0.x) is now true on *success*. Search for all uses
  of the drawing and image filter results and write them as `... != 0` (error) or `... == 0` (success).
- **Coordinates and sizes are `Sint16`** instead of `float` (for example `pixelRGBA`, `lineRGBA`, `circleRGBA`,
  the polygon vertex arrays `const Sint16 *vx, const Sint16 *vy`). Round coordinates to integers; the arrays of
  polygon functions must be `Sint16` arrays. Values outside -32768..32767 do not fit.
- **Image filters:** `SDL_imageFilterMultNor` exists again; `SDL_imageFilterMultUnbound` and
  `SDL_imageFilterMultInv` remain. `SDL_imageFilterShiftRightUint` and `SDL_imageFilterShiftLeftUint` no longer
  take a `bpp` parameter (they work on 32-bit words). `Mult`, `MultDivby2`, `MultDivby4`, `MultByByte`,
  `ShiftRightAndMultByByte`, `Div`, `Mean` and `NormalizeLinear` compute the documented SDL2_gfx results again
  (for example `Mult` = `min(S1 * S2, 255)` instead of `S1 * S2 / 255`; `Div` by 0 = 255).
- **`...Color` functions** decode the color as `0xRRGGBBAA`; 1.0.x used the bytes in memory order (`0xAABBGGRR` on
  little-endian machines). Change the constants or build with `-DSDL3GFX_LEGACY_COLOR_BYTE_ORDER=ON`.
- **`SDL_framerateDelay`** returns `Uint32` instead of `Uint64`.
- **Circles and ellipses with radii between 0 and 1** no longer exist as a special case: radii are integers
  (radius 0 draws a pixel).
- **Build:** headers install to `include/SDL3_gfx` as before, the CMake targets are new (see the table above) and
  the shared library is `libSDL3_gfx.so.2` (1.0.x: `.so.1`), so both versions can be installed side by side.