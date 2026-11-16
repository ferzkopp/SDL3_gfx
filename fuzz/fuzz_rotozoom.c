/*
 * Fuzz target: SDL3_rotozoom. Builds a small source surface (8, 24 or 32 bit, optional color key)
 * from the input and runs every transformation with arbitrary angles/zoom factors (incl. NaN,
 * infinity, zero and negative values). Zoom factors are limited so results stay below 4096x4096.
 */
#include "fuzz_common.h"

#include <math.h>

#include "SDL3_rotozoom.h"

static double limit_zoom(double z)
{
	if (z > 16.0) {
		return 16.0;
	}
	if (z < -16.0) {
		return -16.0;
	}
	return z; /* NaN passes through on purpose */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzReader in;
	SDL_Surface *src, *dst;
	int w, h, depth, x, y, dw, dh;
	Uint8 fmt;
	double angle, zx, zy;

	fuzz_init(&in, data, size);
	w = 1 + fuzz_u8(&in) % 64;
	h = 1 + fuzz_u8(&in) % 64;
	fmt = fuzz_u8(&in);
	depth = (fmt % 3 == 0) ? 8 : ((fmt % 3 == 1) ? 24 : 32);
	if (depth == 8) {
		src = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_INDEX8);
		if ((src != NULL) && (SDL_CreateSurfacePalette(src) == NULL)) {
			SDL_DestroySurface(src);
			src = NULL;
		}
	} else if (depth == 24) {
		src = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
	} else {
		src = SDL_CreateSurface(w, h, (fmt & 4) ? SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_ARGB8888);
	}
	if (src == NULL) {
		return 0;
	}
	for (y = 0; y < h; y++) {
		Uint8 *row = (Uint8 *)src->pixels + y * src->pitch;
		for (x = 0; x < w * SDL_BYTESPERPIXEL(src->format); x++) {
			row[x] = fuzz_u8(&in);
		}
	}
	if (fmt & 8) {
		SDL_SetSurfaceColorKey(src, true, fuzz_u32(&in));
	}

	angle = fuzz_double(&in);
	zx = limit_zoom(fuzz_double(&in));
	zy = limit_zoom(fuzz_double(&in));

	rotozoomSurfaceSize(w, h, angle, zx, &dw, &dh);
	rotozoomSurfaceSizeXY(w, h, angle, zx, zy, &dw, &dh);
	zoomSurfaceSize(w, h, zx, zy, &dw, &dh);

	dst = rotozoomSurface(src, angle, zx, fmt & 16 ? SMOOTHING_ON : SMOOTHING_OFF);
	SDL_DestroySurface(dst);
	dst = rotozoomSurfaceXY(src, angle, zx, zy, fmt & 32 ? SMOOTHING_ON : SMOOTHING_OFF);
	SDL_DestroySurface(dst);
	dst = zoomSurface(src, zx, zy, fmt & 64 ? SMOOTHING_ON : SMOOTHING_OFF);
	SDL_DestroySurface(dst);
	dst = shrinkSurface(src, 1 + fuzz_u8(&in) % 8, 1 + fuzz_u8(&in) % 8);
	SDL_DestroySurface(dst);
	dst = rotateSurface90Degrees(src, (Sint8)fuzz_u8(&in));
	SDL_DestroySurface(dst);

	SDL_DestroySurface(src);
	return 0;
}
