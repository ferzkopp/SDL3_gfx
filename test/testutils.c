#include "testutils.h"

#include "SDL3_gfxPrimitives.h"

GfxTestOptions gfxtest_options = {GFXTEST_GOLDEN_DIR, GFXTEST_DATA_DIR, "failed", 0};

int GfxTest_CreateCanvas(GfxTestCanvas *canvas, int w, int h)
{
	canvas->renderer = NULL;
	canvas->surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
	if (canvas->surface == NULL) {
		SDLTest_AssertCheck(0, "SDL_CreateSurface: %s", SDL_GetError());
		return -1;
	}
	canvas->renderer = SDL_CreateSoftwareRenderer(canvas->surface);
	if (canvas->renderer == NULL) {
		SDLTest_AssertCheck(0, "SDL_CreateSoftwareRenderer: %s", SDL_GetError());
		SDL_DestroySurface(canvas->surface);
		canvas->surface = NULL;
		return -1;
	}
	GfxTest_ClearCanvas(canvas, 0, 0, 0);
	return 0;
}

void GfxTest_ClearCanvas(GfxTestCanvas *canvas, Uint8 r, Uint8 g, Uint8 b)
{
	SDL_SetRenderDrawBlendMode(canvas->renderer, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(canvas->renderer, r, g, b, 255);
	SDL_RenderClear(canvas->renderer);
	SDL_FlushRenderer(canvas->renderer);
}

void GfxTest_DestroyCanvas(GfxTestCanvas *canvas)
{
	/* The font texture cache is global and belongs to the renderer (finding FONT-001): flush it
	   while the renderer is still alive. */
	gfxPrimitivesSetFont(NULL, 0, 0);
	if (canvas->renderer) {
		SDL_DestroyRenderer(canvas->renderer);
		canvas->renderer = NULL;
	}
	if (canvas->surface) {
		SDL_DestroySurface(canvas->surface);
		canvas->surface = NULL;
	}
}

Uint32 GfxTest_GetPixel(GfxTestCanvas *canvas, int x, int y)
{
	SDL_Surface *s = canvas->surface;
	SDL_FlushRenderer(canvas->renderer);
	if (x < 0 || y < 0 || x >= s->w || y >= s->h) {
		return 0;
	}
	return *(Uint32 *)((Uint8 *)s->pixels + y * s->pitch + x * 4);
}

int GfxTest_CountPixelsNot(GfxTestCanvas *canvas, Uint32 argb)
{
	int x, y, count = 0;
	SDL_Surface *s = canvas->surface;
	SDL_FlushRenderer(canvas->renderer);
	for (y = 0; y < s->h; y++) {
		const Uint32 *row = (const Uint32 *)((const Uint8 *)s->pixels + y * s->pitch);
		for (x = 0; x < s->w; x++) {
			if (row[x] != argb) {
				count++;
			}
		}
	}
	return count;
}

const char *GfxTest_Path(char *buf, size_t size, const char *dir, const char *file)
{
	SDL_snprintf(buf, size, "%s/%s", dir, file);
	return buf;
}

SDL_Surface *GfxTest_LoadData(const char *file)
{
	char path[1024];
	SDL_Surface *s = SDL_LoadBMP(GfxTest_Path(path, sizeof(path), gfxtest_options.data_dir, file));
	SDLTest_AssertCheck(s != NULL, "Load %s: %s", path, s ? "ok" : SDL_GetError());
	return s;
}

/* Golden images are stored as 24-bit BMP (the canvas is always opaque). */
static int save_rgb24(SDL_Surface *src, const char *path)
{
	int result;
	SDL_Surface *rgb = SDL_ConvertSurface(src, SDL_PIXELFORMAT_BGR24);
	if (rgb == NULL) {
		return -1;
	}
	result = SDL_SaveBMP(rgb, path) ? 0 : -1;
	SDL_DestroySurface(rgb);
	return result;
}

int GfxTest_CompareGolden(GfxTestCanvas *canvas, const char *name)
{
	char file[256], path[1024];
	SDL_Surface *golden, *expected;
	int result;

	SDL_FlushRenderer(canvas->renderer);
	SDL_snprintf(file, sizeof(file), "%s.bmp", name);
	GfxTest_Path(path, sizeof(path), gfxtest_options.golden_dir, file);

	if (gfxtest_options.generate) {
		result = save_rgb24(canvas->surface, path);
		SDLTest_AssertCheck(result == 0, "Write golden image %s", path);
		return result;
	}

	golden = SDL_LoadBMP(path);
	if (!SDLTest_AssertCheck(golden != NULL, "Load golden image %s", path)) {
		return -1;
	}
	expected = SDL_ConvertSurface(golden, SDL_PIXELFORMAT_ARGB8888);
	SDL_DestroySurface(golden);
	if (!SDLTest_AssertCheck(expected != NULL, "Convert golden image %s", path)) {
		return -1;
	}

	result = SDLTest_CompareSurfaces(canvas->surface, expected, 0);
	SDLTest_AssertCheck(result == 0, "Canvas matches golden image %s (result %d)", file, result);
	if (result != 0) {
		char out[1024];
		SDL_snprintf(out, sizeof(out), "%s/%s", gfxtest_options.output_dir, file);
		if (save_rgb24(canvas->surface, out) == 0) {
			SDLTest_Log("Actual image written to %s", out);
		}
	}
	SDL_DestroySurface(expected);
	return result;
}
