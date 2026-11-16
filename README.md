# SDL3_gfx

Graphics primitives (line, box, circle, polygon, ...), surface rotozoomer, framerate control and
image filters for [SDL3](https://github.com/libsdl-org/SDL).

![Screenshot of Filled Circle Test](Screenshots/FilledCircle.png)

SDL3_gfx 2.x has the API of [SDL2_gfx](https://github.com/ferzkopp/SDL2_gfx) and draws the same pixels, so
SDL2_gfx code ports to SDL3 by changing includes and link targets. The 1.0.x versions of this repository
(the first SDL3 port) used a different API with `float` coordinates and `bool` results; see
[docs/MIGRATION.md](docs/MIGRATION.md).

## Components

| Component | Header | Description |
|---|---|---|
| Graphics primitives | `SDL3_gfxPrimitives.h` | Pixels, lines (plain, anti-aliased, thick), rectangles, rounded boxes, circles, arcs, ellipses, pies, triangles, polygons (filled, textured), Bézier curves, 8x8 bitmap font text — drawn with an `SDL_Renderer` |
| Rotozoomer | `SDL3_rotozoom.h` | Rotate, zoom, shrink and flip `SDL_Surface`s (with optional smoothing) |
| Framerate control | `SDL3_framerate.h` | Simple frame-rate limiter (`FPSmanager`) |
| Image filters | `SDL3_imageFilter.h` | Byte-buffer arithmetic and filter functions (add, mean, multiply, shift, binarize, ...) |

SDL3_gfx needs SDL 3.2.0 or newer (for SDL2 use [SDL2_gfx](https://github.com/ferzkopp/SDL2_gfx), for SDL 1.2
use [SDL_gfx](https://github.com/ferzkopp/SDL_gfx)).

## Quick start

Build with CMake against an installed SDL3 (see [docs/BUILDING.md](docs/BUILDING.md) for the full
Windows/Linux setup, presets, Emscripten and VS Code integration):

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/SDL3
cmake --build build
ctest --test-dir build
cmake --install build --prefix /path/to/install
```

Use it from CMake:

```cmake
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_gfx REQUIRED CONFIG)
target_link_libraries(app PRIVATE SDL3_gfx::SDL3_gfx SDL3::SDL3)
```

```c
#include <SDL3/SDL.h>
#include <SDL3_gfx/SDL3_gfxPrimitives.h>

/* ... after creating an SDL_Renderer: */
filledCircleRGBA(renderer, 160, 120, 50, 255, 128, 0, 255);
aalineRGBA(renderer, 0, 0, 319, 239, 255, 255, 255, 255);
stringRGBA(renderer, 10, 10, "Hello SDL3_gfx", 255, 255, 255, 255);
SDL_RenderPresent(renderer);
```

Drawing functions return 0 on success and -1 on error (like SDL2_gfx; SDL3 functions return `true` on success).
The error message is available with `SDL_GetError()`.

With pkg-config: `cc app.c $(pkg-config --cflags --libs sdl3 sdl3-gfx)`.

## Usage notes

- Colors of the `...Color` functions are `0xRRGGBBAA` (SDL3_gfx 1.0.x used the bytes in memory order, see the
  `SDL3GFX_LEGACY_COLOR_BYTE_ORDER` build option in [docs/MIGRATION.md](docs/MIGRATION.md)).
- All drawing functions are thread-safe with respect to SDL3_gfx itself (no global buffers); the usual SDL rules
  for using a renderer from one thread apply. The text functions cache one glyph texture per character for the
  renderer last used: call `gfxPrimitivesSetFont(NULL, 0, 0)` before destroying that renderer.
- Primitives batch their SDL draw calls (one call per primitive instead of one per pixel or span), which makes
  them much faster with hardware renderers; see [docs/PERFORMANCE.md](docs/PERFORMANCE.md). The hint
  `SDL3_GFX_AA_GEOMETRY` (`0`/`1`) controls how anti-aliased pixels are drawn.
- With a render scale below 1, SDL3's software renderer does not draw lines and rectangles that become thinner
  than one pixel, so most primitives disappear there (SDL2 drew them one pixel wide; PRIM-017 in
  [docs/CODE_REVIEW.md](docs/CODE_REVIEW.md)). Hardware renderers are not affected.
- Temporary memory is allocated with `SDL_malloc` and honors custom SDL memory functions.

## Demos and tests

- `demo/` contains interactive programs (`testgfx`, `testrotozoom`, `testframerate`,
  `testimagefilter`) that show and benchmark the library; run them with `--help` for options.
- `test/` contains the automated CTest suites (`ctest --preset win-debug`). The golden images are the same as
  in SDL2_gfx; with an SDL2_gfx checkout next to this one, the `parity` tests also compare against it.

## Documentation

API documentation is in the public headers and source files (Doxygen comments); generate the HTML reference with
`cmake --build --preset win-release --target docs`. Build instructions: [docs/BUILDING.md](docs/BUILDING.md).
Porting from SDL2_gfx or SDL3_gfx 1.0.x: [docs/MIGRATION.md](docs/MIGRATION.md). Release history:
[CHANGELOG.md](CHANGELOG.md). Code review findings and fixes: [docs/CODE_REVIEW.md](docs/CODE_REVIEW.md).
Performance measurements: [docs/PERFORMANCE.md](docs/PERFORMANCE.md).

## Contributing

Bug reports and pull requests are welcome on [GitHub](https://github.com/ferzkopp/SDL3_gfx/issues). Please run the
tests (`ctest`) and the formatter (`scripts/format.ps1` or `scripts/format.sh`) before submitting; behavior changes
need a test and, for drawing changes, regenerated golden images (see [docs/BUILDING.md](docs/BUILDING.md#tests)).
Changes to the drawing code should also go to SDL2_gfx so that both libraries keep drawing the same pixels.

## Credits

SDL3_gfx is written and maintained by Andreas Schiffler. It is the SDL3 version of SDL2_gfx and SDL_gfx; the
contributors to SDL_gfx over the years are listed in the [SDL_gfx README](https://github.com/ferzkopp/SDL_gfx#contributors).

The first SDL3 port (versions 1.0.0 and 1.0.1, the start of this repository's history) was created by
Sulaiman Abdul-Khabir ([sabdul-khabir/SDL3_gfx](https://github.com/sabdul-khabir/SDL3_gfx)) with contributions by
Italo F. Capasso B., Duilio Pérez, Sid. B. and hdejin. Its fixes and additions were reviewed for 2.0; the image
filters `SDL_imageFilterMultUnbound` and `SDL_imageFilterMultInv` come from that port.

The anti-aliased line code is based on the Wu algorithm as described by Michael Abrash.

## License

zlib license — see [LICENSE](LICENSE).

Andreas Schiffler — aschiffler at ferzkopp dot net — <https://www.ferzkopp.net>