# SDL3_gfx performance

Measured with `bench/gfxbench` (see [BUILDING.md](BUILDING.md#benchmarks)): random shapes on a
640x480 target; rotozoom on a 256x256 32-bit surface; image filters on 1 MB buffers. SDL3_gfx contains the
optimizations of SDL2_gfx (draw call batching PERF-002, visible-rows-only polygon fills PERF-001); see the
SDL2_gfx [PERFORMANCE.md](https://github.com/ferzkopp/SDL2_gfx/blob/main/docs/PERFORMANCE.md) for their effect.
Raw data of the SDL3_gfx run: `docs/benchmarks/baseline-software.csv` and `docs/benchmarks/baseline-d3d11.csv`.

## SDL3_gfx 2.0 compared with SDL2_gfx 1.1 (2026-11-15)

Same drawing code, SDL 3.4.20 versus SDL 2.32.10, MSVC 19.51 RelWithDebInfo x64, AMD Ryzen 9 5950X 16-Core
Processor, NVIDIA GeForce RTX 3060. Interleaved runs of both libraries, `gfxbench --time 0.5`, mean of two rounds;
operations per second (image filters: MB/s). Selected items:

| Group | Item | Software SDL2_gfx | SDL3_gfx | | Direct3D 11 SDL2_gfx | SDL3_gfx | |
|---|---|---:|---:|---:|---:|---:|---:|
| primitive | pixel | 10,240,523 | 8,632,962 | 0.84x | 1,351,468 | 3,377,699 | 2.50x |
| primitive | hline | 6,750,964 | 6,470,317 | 0.96x | 1,173,822 | 2,561,672 | 2.18x |
| primitive | box | 29,543 | 29,762 | 1.01x | 358,329 | 677,111 | 1.89x |
| primitive | line | 1,845,071 | 2,106,338 | 1.14x | 336,182 | 355,212 | 1.06x |
| primitive | aaline | 14,840 | 14,754 | 0.99x | 29,288 | 21,941 | 0.75x |
| primitive | thickLine(5) | 35,172 | 52,117 | 1.48x | 59,011 | 65,938 | 1.12x |
| primitive | circle | 421,429 | 448,966 | 1.07x | 164,956 | 249,269 | 1.51x |
| primitive | aacircle | 14,246 | 16,839 | 1.18x | 22,323 | 22,884 | 1.03x |
| primitive | filledCircle | 72,334 | 83,933 | 1.16x | 103,409 | 116,619 | 1.13x |
| primitive | arc | 744,138 | 896,800 | 1.21x | 335,516 | 404,606 | 1.21x |
| primitive | aatrigon | 5,266 | 3,490 | 0.66x | 7,484 | 7,463 | 1.00x |
| primitive | aapolygon(8) | 8,658 | 5,092 | 0.59x | 13,387 | 12,019 | 0.90x |
| primitive | filledPolygon(8) | 33,842 | 43,617 | 1.29x | 44,571 | 44,208 | 0.99x |
| primitive | filledPolygon(8) (blend) | 22,688 | 27,033 | 1.19x | 44,509 | 41,488 | 0.93x |
| primitive | texturedPolygon(8) | 19,117 | 12,554 | 0.66x | 4,852 | 3,715 | 0.77x |
| primitive | string(19) | 100,917 | 81,289 | 0.81x | 80,283 | 113,428 | 1.41x |
| rotozoom | rotozoom 30deg x1 | 3,168 | 5,456 | 1.72x | 3,310 | 5,205 | 1.57x |
| rotozoom | rotozoom 30deg x1 smooth | 994 | 1,155 | 1.16x | 1,002 | 1,146 | 1.14x |
| rotozoom | zoom x2 smooth | 432 | 443 | 1.03x | 435 | 454 | 1.04x |
| imagefilter | Add | 673 | 829 | 1.23x | 676 | 816 | 1.21x |
| imagefilter | AbsDiff | 23,827 | 20,374 | 0.86x | 23,967 | 20,700 | 0.86x |
| imagefilter | BinarizeUsingThreshold | 48,794 | 47,546 | 0.97x | 47,478 | 47,526 | 1.00x |

### Observations

- Hardware renderers: simple primitives (`pixel`, `hline`, `box`, outlines) are 1.5-2.5x faster with SDL3,
  whose Direct3D 11 renderer has less overhead per draw call.
- Anti-aliased lines and polygons are slower with SDL3 (PERF-003, open); `aacircle` and `aaellipse` are not. The anti-aliased pixels are drawn
  with one `SDL_RenderGeometryRaw` call per 64 pixels on hardware renderers (SDL3 takes float `SDL_FColor`
  vertex colors) and with `SDL_SetRenderDrawColor` + `SDL_RenderPoint` per pixel on the software renderer;
  both are more expensive per pixel in SDL3. Larger geometry batches (256 pixels per call) did not help.
  Measured in isolation with `--time 1.5` (three rounds): `aaline` 0.75x (software and Direct3D 11),
  `aapolygon(8)` 0.6x.
- `texturedPolygon(8)` on the software renderer is 0.7x (texture creation and one `SDL_RenderTexture` per
  span). On Direct3D 11 it is 1.5x faster in isolation (12,400 versus 8,200 ops/s); the 0.77x of the full run
  comes from the order of the items (as in SDL2_gfx, earlier items slow down later rendering).
- The software renderer is otherwise within ±20% of SDL2 (`pixel` 0.84x, `string` 0.81x; outlines and fills
  1.1-1.5x faster).
- Rotozoom and image filters do not use SDL for the pixel work; the differences are compiler/noise effects
  (`rotozoom 30deg x1` 1.6-1.7x faster in both runs).