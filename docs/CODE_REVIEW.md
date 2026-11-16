# SDL3_gfx code review

Living document of the code review of SDL3_gfx. Each finding has an ID that is referenced by the commit that fixes
it and, where applicable, by a test case.

SDL3_gfx 2.0 is the SDL3 version of SDL2_gfx 1.1: the drawing, rotozoom, framerate and image filter code is shared
and draws the same pixels (the golden images are identical). The findings of that code, with their IDs, tests and
fixes, are documented in the code review of [SDL2_gfx](https://github.com/ferzkopp/SDL2_gfx/blob/main/docs/CODE_REVIEW.md)
(API-001, BLD-001, FONT-001/002, FRM-001/002, IMGF-001 to IMGF-006, PERF-001/002, PRIM-001 to PRIM-016,
ROTO-001 to ROTO-005, SA-001 to SA-004, TOOL-001); all of them are fixed here as well. This document covers the
SDL3 port.

## Provenance

- The history starts with the first SDL3 port by Sulaiman Abdul-Khabir and contributors
  (`sabdul-khabir/SDL3_gfx`, 1.0.0 and 1.0.1, tag `v1.0.1`, plus unreleased changes up to 2026-02-02). It changed
  the SDL2_gfx API to `float` coordinates and `bool` results.
- Version 2.0 replaces the sources with the modernized SDL2_gfx code ported to SDL3 and restores the SDL2_gfx API
  (decision D8; see [MIGRATION.md](MIGRATION.md)). The changes of the first port were reviewed (below).

## Method

| Tool / technique | Result |
|---|---|
| MSVC 19.51 `/W4` | 0 warnings (library, tests, demos, benchmark, fuzzers) |
| MSVC `/analyze`, clang-tidy 23, cppcheck 2.22 (`scripts/analyze.ps1`) | 0 / 0 / 0 |
| Golden-image and unit tests (`test/`), SDL3 3.4.20 | all pass with the unchanged SDL2_gfx golden images (pixel-exact) |
| `parity` tests against an SDL2_gfx checkout | scenes and public headers identical (`scripts/check_api_parity.py`: 102 functions, plus 2 additions) |
| Hardware renderer tests (`PrimitivesGPU`) | direct3d, direct3d11, direct3d12, opengl, opengles2 and SDL3's `gpu` renderer match the software renderer within ±2 |
| libFuzzer + ASan (`fuzz/`), 2 min per target | clean: primitives 136k, rotozoom 502k, imagefilter 2.2M, font 23k executions |
| Manual review of the first SDL3 port | REF-001 to REF-003 |
| Benchmark against SDL2_gfx (`bench/gfxbench`) | PERF-003 |

## Findings

Severity and status as in the SDL2_gfx review.

| ID | Sev. | Area | Summary | Status |
|---|---|---|---|---|
| REF-001 | high | imageFilter | The image filter changes of the first port (PR #13) changed documented results and contain bugs: `Mult` multiplies `Src1` by itself (`fSrc2 = cursrc1[i]`), `Div` divides the integer bytes before converting to float (results only 0 or 255; division by 0 gives 0 instead of 255), the `...Uint` shifts with the new `bpp` parameter combine the bytes 4 bits apart (`sizeof(unsigned int) * j`) and write `(result >> bpp)` to every byte, `MultByByte`/`ShiftRightAndMultByByte` became normalized multiplications, and the copy shortcuts still had the swapped `memcpy(Src1, Dest, ...)` of SDL2_gfx 1.0.4 (IMGF-006). | fixed ("Port the modernized SDL2_gfx to SDL3 with the SDL2_gfx API") — SDL2_gfx filters (IMGF-001 to IMGF-006 fixed, in-place safe); `MultUnbound` and `MultInv` kept as additions in integer math |
| REF-002 | medium | primitives | The `float` API (PR #18) truncates coordinates in the integer algorithms and needed special cases such as radii between 0 and 1 (PR #20, PRIM-003). | fixed (same commit) — `Sint16` API (D8); PRIM-003 does not apply to integer radii |
| REF-003 | low | API | `bool` results (PRIM/IMGF, PR #17) invert the meaning of the SDL2_gfx `int` results for ported code. | fixed (same commit) — `int` 0/-1 results again; SDL3 results are mapped with `GFX_RESULT` |
| PRIM-017 | low | primitives | With a render scale below 1, SDL3's software renderer truncates rectangles (and lines drawn as rectangles) to 0 pixels when they become thinner than one pixel (`SW_QueueFillRects`: `(int)rects->h`); SDL2 enforced at least one pixel. All primitive spans are one logical pixel high, so primitives disappear in this case. | wontfix (SDL3 behavior; hardware renderers are not affected) — documented in the README and MIGRATION.md; test `api_polygon_viewport_clip` checks the logical visible area instead of pixels |
| ROTO-005 | low | rotozoom | SDL3 surfaces have no default palette: indexed results need their own copy of the source palette (in SDL2_gfx, `rotateSurface90Degrees` gave 8-bit results a white palette). | fixed — `_rotozoomCreateSurface` copies the palette with the same number of colors; test `rotozoom_rotate90_palette` |
| PERF-003 | low | primitives | Anti-aliased lines and polygons are slower than SDL2_gfx on SDL2 (`aaline` 0.75x, `aapolygon` 0.6x) (same code; SDL3's per-pixel cost in `SDL_RenderGeometryRaw` with float colors and in `SDL_RenderPoint`); `texturedPolygon` on the software renderer 0.7x. See [PERFORMANCE.md](PERFORMANCE.md). | open — larger geometry batches did not help |

## Decisions

- **D8:** SDL3_gfx keeps the SDL2_gfx API (`Sint16`, `int` 0/-1, same names) with an SDL3 implementation: float
  render API, `SDL_FColor` geometry, `SDL_CreateSurface`/`SDL_GetPixelFormatDetails`/`SDL_CreateSurfacePalette`,
  software renderer detection with `SDL_GetRendererName`, nearest scaling for glyph and polygon textures (SDL3
  textures default to linear scaling).
- **Q3:** `SDL_imageFilterMultNor` is restored; `SDL_imageFilterMultUnbound` (identical) and `SDL_imageFilterMultInv`
  (`255 - (255 - S1) * (255 - S2) / 255`, rounded) remain as additions.
- **Q4:** `FPSmanager` uses `Uint64` tick fields; `SDL_framerateDelay` returns `Uint32` as in SDL2_gfx.
- **Q5:** the `SDL_`-prefixed functions (`SDL_initFramerate`, `SDL_imageFilter...`) keep their names.
- **Version 2.0.0, SONAME `libSDL3_gfx.so.2`:** incompatible with the 1.0.x API (`.so.1`).
- **Color byte order:** `0xRRGGBBAA` as in SDL2_gfx 1.1; `SDL3GFX_LEGACY_COLOR_BYTE_ORDER` restores the memory byte
  order of SDL3_gfx 1.0.x.