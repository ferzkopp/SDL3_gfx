/*
 * Shared helpers for the SDL3_gfx automated tests (SDL3_test based).
 *
 * A "canvas" is a 32-bit ARGB surface with a software renderer drawing into it, so tests run
 * headless (no window, no GPU) and are deterministic for a given SDL version.
 */
#ifndef SDL3_GFX_TESTUTILS_H
#define SDL3_GFX_TESTUTILS_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

#define GFXTEST_CANVAS_W 64
#define GFXTEST_CANVAS_H 64

typedef struct GfxTestCanvas {
	SDL_Surface *surface;
	SDL_Renderer *renderer;
} GfxTestCanvas;

/* Command line options shared by all suites (set by main). */
typedef struct GfxTestOptions {
	const char *golden_dir; /* reference images */
	const char *data_dir; /* sample images (test/data) */
	const char *output_dir; /* actual images of failed comparisons */
	int generate; /* write golden images instead of comparing */
} GfxTestOptions;

extern GfxTestOptions gfxtest_options;

/* Creates a canvas cleared to opaque black. Returns 0 on success. */
int GfxTest_CreateCanvas(GfxTestCanvas *canvas, int w, int h);
void GfxTest_ClearCanvas(GfxTestCanvas *canvas, Uint8 r, Uint8 g, Uint8 b);
/* Destroys the renderer and surface. Flushes the SDL3_gfx font texture cache first. */
void GfxTest_DestroyCanvas(GfxTestCanvas *canvas);

/* Reads the pixel at (x, y) of the canvas as 0xAARRGGBB (after flushing pending render commands). */
Uint32 GfxTest_GetPixel(GfxTestCanvas *canvas, int x, int y);
/* Counts the pixels that differ from the given 0xAARRGGBB value. */
int GfxTest_CountPixelsNot(GfxTestCanvas *canvas, Uint32 argb);

/*
 * Compares the canvas with golden/<name>.bmp (or writes it in --generate mode).
 * Records an SDL_test assertion; returns 0 if the images match.
 */
int GfxTest_CompareGolden(GfxTestCanvas *canvas, const char *name);

/* Loads test/data/<file> (caller frees). */
SDL_Surface *GfxTest_LoadData(const char *file);

/* Builds "<dir>/<file>" into buf. */
const char *GfxTest_Path(char *buf, size_t size, const char *dir, const char *file);

#endif
