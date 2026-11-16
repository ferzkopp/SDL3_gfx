/*
 * Tests for SDL3_rotozoom: size calculations, exact pixel mappings (identity, flips, 90 degree
 * rotations, integer shrinking), format handling (8-bit palettes, 24/32-bit) and golden images
 * for smoothed rotation/zoom.
 */
#include <math.h>

#include "testutils.h"

#include "SDL3_rotozoom.h"

/* 32-bit ABGR test pattern with unique pixel values (format used by the rotozoomer internally). */
static SDL_Surface *create_pattern32(int w, int h)
{
	int x, y;
	SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ABGR8888);
	if (s == NULL) {
		return NULL;
	}
	for (y = 0; y < h; y++) {
		Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
		for (x = 0; x < w; x++) {
			row[x] = 0xFF000000u | (Uint32)((x * 255 / (w - 1)) << 16) | (Uint32)((y * 255 / (h - 1)) << 8) | (Uint32)((x ^ y) & 0xFF);
		}
	}
	return s;
}

static Uint32 get32(SDL_Surface *s, int x, int y)
{
	return *(Uint32 *)((Uint8 *)s->pixels + y * s->pitch + x * 4);
}

static int test_size_functions(void *arg)
{
	int w, h;
	(void)arg;
	zoomSurfaceSize(10, 20, 2.0, 3.0, &w, &h);
	SDLTest_AssertCheck(w == 20 && h == 60, "zoomSurfaceSize 10x20 * (2,3) = %dx%d (expected 20x60)", w, h);
	zoomSurfaceSize(10, 20, 0.5, 0.5, &w, &h);
	SDLTest_AssertCheck(w == 5 && h == 10, "zoomSurfaceSize 10x20 * 0.5 = %dx%d (expected 5x10)", w, h);
	zoomSurfaceSize(10, 20, -1.0, -1.0, &w, &h);
	SDLTest_AssertCheck(w == 10 && h == 20, "zoomSurfaceSize with negative (flip) factors = %dx%d (expected 10x20)", w, h);
	zoomSurfaceSize(10, 20, 0.0, 0.0, &w, &h);
	SDLTest_AssertCheck(w >= 1 && h >= 1, "zoomSurfaceSize with zero factors gives at least 1x1 (%dx%d)", w, h);
	rotozoomSurfaceSize(10, 20, 0.0, 1.0, &w, &h);
	SDLTest_AssertCheck(w == 10 && h == 20, "rotozoomSurfaceSize angle 0 = %dx%d (expected 10x20)", w, h);
	rotozoomSurfaceSize(10, 20, 90.0, 1.0, &w, &h);
	SDLTest_AssertCheck(w >= 20 && w <= 22 && h >= 10 && h <= 12, "rotozoomSurfaceSize angle 90 = %dx%d (expected ~20x10)", w, h);
	rotozoomSurfaceSize(10, 10, 45.0, 1.0, &w, &h);
	SDLTest_AssertCheck(w >= 14 && w <= 16 && h >= 14 && h <= 16, "rotozoomSurfaceSize 10x10 angle 45 = %dx%d (expected ~15x15)", w, h);
	return TEST_COMPLETED;
}

/* ROTO-001: zoomx/zoomy were mixed up in _rotozoomSurfaceSizeTrig. */
static int test_size_xy(void *arg)
{
	int w, h;
	(void)arg;
	rotozoomSurfaceSizeXY(10, 20, 0.0, 2.0, 0.5, &w, &h);
	SDLTest_AssertCheck(w == 20 && h == 10, "rotozoomSurfaceSizeXY angle 0 zoom (2,0.5) = %dx%d (expected 20x10)", w, h);
	rotozoomSurfaceSizeXY(10, 20, 90.0, 2.0, 0.5, &w, &h);
	SDLTest_AssertCheck(w >= 10 && w <= 12 && h >= 20 && h <= 22, "rotozoomSurfaceSizeXY angle 90 zoom (2,0.5) = %dx%d (expected ~10x20)", w, h);
	return TEST_COMPLETED;
}

static int test_zoom_identity_and_flip(void *arg)
{
	SDL_Surface *src, *dst;
	int x, y, errors;
	(void)arg;
	src = create_pattern32(17, 13);
	if (!SDLTest_AssertCheck(src != NULL, "Create pattern")) {
		return TEST_ABORTED;
	}

	dst = zoomSurface(src, 1.0, 1.0, SMOOTHING_OFF);
	SDLTest_AssertCheck(dst && dst->w == 17 && dst->h == 13, "zoom 1.0 keeps the size");
	errors = 0;
	for (y = 0; dst && y < 13; y++) {
		for (x = 0; x < 17; x++) {
			errors += get32(dst, x, y) != get32(src, x, y);
		}
	}
	SDLTest_AssertCheck(dst && errors == 0, "zoom 1.0 is the identity (%d differing pixels)", errors);
	SDL_DestroySurface(dst);

	dst = zoomSurface(src, -1.0, 1.0, SMOOTHING_OFF);
	errors = 0;
	for (y = 0; dst && y < 13; y++) {
		for (x = 0; x < 17; x++) {
			errors += get32(dst, x, y) != get32(src, 16 - x, y);
		}
	}
	SDLTest_AssertCheck(dst && errors == 0, "zoom (-1,1) mirrors horizontally (%d differing pixels)", errors);
	SDL_DestroySurface(dst);

	dst = zoomSurface(src, 1.0, -1.0, SMOOTHING_OFF);
	errors = 0;
	for (y = 0; dst && y < 13; y++) {
		for (x = 0; x < 17; x++) {
			errors += get32(dst, x, y) != get32(src, x, 12 - y);
		}
	}
	SDLTest_AssertCheck(dst && errors == 0, "zoom (1,-1) mirrors vertically (%d differing pixels)", errors);
	SDL_DestroySurface(dst);

	dst = zoomSurface(src, 2.0, 2.0, SMOOTHING_OFF);
	errors = 0;
	for (y = 0; dst && y < 26; y++) {
		for (x = 0; x < 34; x++) {
			errors += get32(dst, x, y) != get32(src, x / 2, y / 2);
		}
	}
	SDLTest_AssertCheck(dst && dst->w == 34 && dst->h == 26 && errors == 0, "zoom 2.0 duplicates pixels (%d differing pixels)", errors);
	SDL_DestroySurface(dst);

	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}

static int test_rotate90(void *arg)
{
	SDL_Surface *src, *r1, *r4, *tmp;
	int x, y, i, errors;
	(void)arg;
	src = create_pattern32(17, 13);
	if (!SDLTest_AssertCheck(src != NULL, "Create pattern")) {
		return TEST_ABORTED;
	}

	r1 = rotateSurface90Degrees(src, 1);
	SDLTest_AssertCheck(r1 && r1->w == 13 && r1->h == 17, "One clockwise turn swaps width and height");
	errors = 0;
	for (y = 0; r1 && y < 13; y++) {
		for (x = 0; x < 17; x++) {
			/* clockwise: src(x, y) -> dst(h-1-y, x) */
			errors += get32(r1, 12 - y, x) != get32(src, x, y);
		}
	}
	SDLTest_AssertCheck(r1 && errors == 0, "One clockwise turn maps pixels correctly (%d differing pixels)", errors);
	SDL_DestroySurface(r1);

	r4 = rotateSurface90Degrees(src, 0);
	for (i = 0; i < 4 && r4; i++) {
		tmp = rotateSurface90Degrees(r4, 1);
		SDL_DestroySurface(r4);
		r4 = tmp;
	}
	errors = 0;
	for (y = 0; r4 && y < 13; y++) {
		for (x = 0; x < 17; x++) {
			errors += get32(r4, x, y) != get32(src, x, y);
		}
	}
	SDLTest_AssertCheck(r4 && errors == 0, "Four turns are the identity (%d differing pixels)", errors);
	SDL_DestroySurface(r4);

	tmp = rotateSurface90Degrees(src, -1);
	SDLTest_AssertCheck(tmp && tmp->w == 13 && tmp->h == 17, "Negative turns (-1 = 3 clockwise) are accepted");
	SDL_DestroySurface(tmp);
	SDLTest_AssertCheck(rotateSurface90Degrees(NULL, 1) == NULL, "NULL source returns NULL");

	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}

/* 8-bit surfaces keep their palette when rotated by 90 degrees (ROTO-005). */
static int test_rotate90_palette(void *arg)
{
	SDL_Surface *src, *dst;
	SDL_Palette *sp, *dp;
	int turns;
	(void)arg;
	src = GfxTest_LoadData("sample8.bmp");
	if (src == NULL) {
		return TEST_ABORTED;
	}
	sp = SDL_GetSurfacePalette(src);
	for (turns = 0; turns < 4; turns++) {
		dst = rotateSurface90Degrees(src, turns);
		dp = dst ? SDL_GetSurfacePalette(dst) : NULL;
		SDLTest_AssertCheck(sp && dp && dp->ncolors == sp->ncolors &&
				SDL_memcmp(dp->colors, sp->colors, (size_t)sp->ncolors * sizeof(SDL_Color)) == 0,
			"%d turns: palette is copied", turns);
		SDL_DestroySurface(dst);
	}
	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}

static int test_shrink(void *arg)
{
	SDL_Surface *src, *dst;
	int x, y;
	(void)arg;
	src = SDL_CreateSurface(8, 6, SDL_PIXELFORMAT_ABGR8888);
	if (!SDLTest_AssertCheck(src != NULL, "Create surface")) {
		return TEST_ABORTED;
	}
	/* 2x2 blocks with values 0 and 200: averages are 100 */
	for (y = 0; y < 6; y++) {
		for (x = 0; x < 8; x++) {
			*(Uint32 *)((Uint8 *)src->pixels + y * src->pitch + x * 4) = ((x + y) & 1) ? 0xFFC8C8C8u : 0xFF000000u;
		}
	}
	dst = shrinkSurface(src, 2, 2);
	SDLTest_AssertCheck(dst && dst->w == 4 && dst->h == 3, "shrinkSurface(2,2) halves the size");
	if (dst) {
		Uint32 p = get32(dst, 1, 1);
		SDLTest_AssertCheck((p & 0x00FFFFFF) == 0x00646464, "shrinkSurface averages 2x2 blocks (0x%08x)", (unsigned int)p);
	}
	SDL_DestroySurface(dst);
	SDLTest_AssertCheck(shrinkSurface(NULL, 2, 2) == NULL, "NULL source returns NULL");
	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}

static int test_formats(void *arg)
{
	SDL_Surface *src8, *src24, *dst;
	(void)arg;
	src8 = GfxTest_LoadData("sample8.bmp");
	src24 = GfxTest_LoadData("sample24.bmp");
	if (src8 == NULL || src24 == NULL) {
		SDL_DestroySurface(src8);
		SDL_DestroySurface(src24);
		return TEST_ABORTED;
	}
	dst = zoomSurface(src8, 0.5, 0.5, SMOOTHING_OFF);
	SDLTest_AssertCheck(dst && SDL_BITSPERPIXEL(dst->format) == 8 && SDL_GetSurfacePalette(dst), "8-bit source stays 8-bit with palette");
	SDLTest_AssertCheck(dst && SDL_GetSurfacePalette(dst) && SDL_GetSurfacePalette(src8) &&
			SDL_GetSurfacePalette(dst)->ncolors == SDL_GetSurfacePalette(src8)->ncolors &&
			SDL_memcmp(SDL_GetSurfacePalette(dst)->colors, SDL_GetSurfacePalette(src8)->colors, SDL_GetSurfacePalette(src8)->ncolors * sizeof(SDL_Color)) == 0,
		"Palette is copied");
	SDL_DestroySurface(dst);
	dst = rotozoomSurface(src8, 30.0, 1.0, SMOOTHING_ON);
	SDLTest_AssertCheck(dst && SDL_BITSPERPIXEL(dst->format) == 8, "8-bit rotozoom result is 8-bit (smoothing ignored)");
	SDL_DestroySurface(dst);
	dst = rotozoomSurface(src24, 30.0, 0.5, SMOOTHING_ON);
	SDLTest_AssertCheck(dst && SDL_BITSPERPIXEL(dst->format) == 32, "24-bit source is converted to 32-bit");
	SDL_DestroySurface(dst);
	SDLTest_AssertCheck(rotozoomSurface(NULL, 30.0, 1.0, SMOOTHING_ON) == NULL, "NULL source returns NULL");
	SDLTest_AssertCheck(zoomSurface(NULL, 1.0, 1.0, SMOOTHING_ON) == NULL, "zoomSurface NULL source returns NULL");
	SDL_DestroySurface(src8);
	SDL_DestroySurface(src24);
	return TEST_COMPLETED;
}

/* 8-bit zoom with flips (ROTO-002: signed step tables) must map pixels like the 32-bit path. */
static int test_zoom_8bit_flip(void *arg)
{
	SDL_Surface *src, *dst;
	int x, y, errors, fx, fy;
	(void)arg;
	src = SDL_CreateSurface(9, 7, SDL_PIXELFORMAT_INDEX8);
	if (!SDLTest_AssertCheck(src != NULL && SDL_CreateSurfacePalette(src) != NULL, "Create 8-bit surface")) {
		SDL_DestroySurface(src);
		return TEST_ABORTED;
	}
	for (y = 0; y < 7; y++) {
		for (x = 0; x < 9; x++) {
			((Uint8 *)src->pixels)[y * src->pitch + x] = (Uint8)(y * 9 + x);
		}
	}
	for (fy = 0; fy < 2; fy++) {
		for (fx = 0; fx < 2; fx++) {
			double zx = fx ? -2.0 : 2.0, zy = fy ? -3.0 : 3.0;
			dst = zoomSurface(src, zx, zy, SMOOTHING_OFF);
			errors = 0;
			for (y = 0; dst && y < 21; y++) {
				for (x = 0; x < 18; x++) {
					int sx = fx ? 8 - x / 2 : x / 2;
					int sy = fy ? 6 - y / 3 : y / 3;
					errors += ((Uint8 *)dst->pixels)[y * dst->pitch + x] != (Uint8)(sy * 9 + sx);
				}
			}
			SDLTest_AssertCheck(dst && dst->w == 18 && dst->h == 21 && errors == 0,
				"8-bit zoom (%g,%g) maps pixels correctly (%d differing pixels)", zx, zy, errors);
			SDL_DestroySurface(dst);
		}
	}
	SDLTest_AssertCheck(shrinkSurface(src, 0, 1) == NULL, "shrinkSurface factorx=0 returns NULL (ROTO-003)");
	SDLTest_AssertCheck(shrinkSurface(src, 1, -2) == NULL, "shrinkSurface factory<0 returns NULL (ROTO-003)");
	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}

/* Rotozoom with different horizontal and vertical zoom factors (ROTO-001). */
static int test_golden_rotozoom_xy(void *arg)
{
	GfxTestCanvas canvas;
	SDL_Surface *src, *tile[4];
	int i;
	(void)arg;
	if (GfxTest_CreateCanvas(&canvas, 64, 64)) {
		return TEST_ABORTED;
	}
	src = create_pattern32(16, 16);
	tile[0] = rotozoomSurfaceXY(src, 30.0, 1.75, 0.75, SMOOTHING_OFF);
	tile[1] = rotozoomSurfaceXY(src, 30.0, 1.75, 0.75, SMOOTHING_ON);
	tile[2] = rotozoomSurfaceXY(src, 90.0, 0.5, 1.5, SMOOTHING_OFF);
	tile[3] = rotozoomSurfaceXY(src, -45.0, -1.25, 0.75, SMOOTHING_ON);
	for (i = 0; i < 4; i++) {
		SDL_Rect dst = {(i % 2) * 32, (i / 2) * 32, 32, 32};
		SDL_Rect clip = {0, 0, 32, 32};
		if (SDLTest_AssertCheck(tile[i] != NULL, "Tile %d created", i)) {
			SDL_SetSurfaceBlendMode(tile[i], SDL_BLENDMODE_NONE);
			SDL_BlitSurface(tile[i], &clip, canvas.surface, &dst);
			SDL_DestroySurface(tile[i]);
		}
	}
	SDL_DestroySurface(src);
	GfxTest_CompareGolden(&canvas, "rotozoom_xy");
	GfxTest_DestroyCanvas(&canvas);
	return TEST_COMPLETED;
}
/* Blits four transformed versions of a 24x24 pattern into the quadrants of a canvas. */
static int test_golden_rotozoom(void *arg)
{
	GfxTestCanvas canvas;
	SDL_Surface *src, *tile[4];
	int i;
	(void)arg;
	if (GfxTest_CreateCanvas(&canvas, 64, 64)) {
		return TEST_ABORTED;
	}
	src = create_pattern32(24, 24);
	tile[0] = rotozoomSurface(src, 30.0, 1.0, SMOOTHING_OFF);
	tile[1] = rotozoomSurface(src, 30.0, 1.0, SMOOTHING_ON);
	tile[2] = rotozoomSurfaceXY(src, 0.0, 1.25, 0.75, SMOOTHING_ON);
	tile[3] = zoomSurface(src, 0.6, 1.3, SMOOTHING_ON);
	for (i = 0; i < 4; i++) {
		SDL_Rect dst = {(i % 2) * 32, (i / 2) * 32, 32, 32};
		SDL_Rect clip = {0, 0, 32, 32};
		if (SDLTest_AssertCheck(tile[i] != NULL, "Tile %d created", i)) {
			SDL_SetSurfaceBlendMode(tile[i], SDL_BLENDMODE_NONE);
			SDL_BlitSurface(tile[i], &clip, canvas.surface, &dst);
			SDL_DestroySurface(tile[i]);
		}
	}
	SDL_DestroySurface(src);
	GfxTest_CompareGolden(&canvas, "rotozoom");
	GfxTest_DestroyCanvas(&canvas);
	return TEST_COMPLETED;
}

/* Non-finite and huge factors are rejected instead of converting NaN/inf/huge sizes to int (ROTO-004). */
static int test_invalid_factors(void *arg)
{
	SDL_Surface *src = create_pattern32(16, 16);
	int w = -1, h = -1;
	(void)arg;
	if (!SDLTest_AssertCheck(src != NULL, "Create pattern")) {
		return TEST_ABORTED;
	}
	SDLTest_AssertCheck(zoomSurface(src, NAN, 1.0, SMOOTHING_OFF) == NULL, "zoomSurface(NaN) returns NULL");
	SDLTest_AssertCheck(zoomSurface(src, 1.0, INFINITY, SMOOTHING_OFF) == NULL, "zoomSurface(inf) returns NULL");
	SDLTest_AssertCheck(zoomSurface(src, 1e12, 1.0, SMOOTHING_OFF) == NULL, "zoomSurface(1e12) returns NULL");
	SDLTest_AssertCheck(rotozoomSurface(src, NAN, 1.0, SMOOTHING_OFF) == NULL, "rotozoomSurface(angle NaN) returns NULL");
	SDLTest_AssertCheck(rotozoomSurface(src, 30.0, INFINITY, SMOOTHING_OFF) == NULL, "rotozoomSurface(zoom inf) returns NULL");
	SDLTest_AssertCheck(rotozoomSurfaceXY(src, 30.0, 1e12, 1.0, SMOOTHING_OFF) == NULL, "rotozoomSurfaceXY(1e12) returns NULL");
	zoomSurfaceSize(16, 16, NAN, 1.0, &w, &h);
	SDLTest_AssertCheck(w == 0 && h == 0, "zoomSurfaceSize(NaN) is 0x0 (%dx%d)", w, h);
	zoomSurfaceSize(16, 16, 1e12, 1.0, &w, &h);
	SDLTest_AssertCheck(w == 0 && h == 0, "zoomSurfaceSize(1e12) is 0x0 (%dx%d)", w, h);
	rotozoomSurfaceSize(16, 16, 30.0, 1e12, &w, &h);
	SDLTest_AssertCheck(w == 0 && h == 0, "rotozoomSurfaceSize(1e12) is 0x0 (%dx%d)", w, h);
	rotozoomSurfaceSizeXY(16, 16, NAN, 1.0, 1.0, &w, &h);
	SDLTest_AssertCheck(w == 0 && h == 0, "rotozoomSurfaceSizeXY(angle NaN) is 0x0 (%dx%d)", w, h);
	zoomSurfaceSize(1000, 1, 100.0, 1.0, &w, &h);
	SDLTest_AssertCheck(w == 100000 && h == 1, "large valid zoom is kept (%dx%d)", w, h);
	SDL_DestroySurface(src);
	return TEST_COMPLETED;
}
static const SDLTest_TestCaseReference sizeRef = {test_size_functions, "rotozoom_size", "Size calculations", TEST_ENABLED};
static const SDLTest_TestCaseReference sizeXYRef = {test_size_xy, "rotozoom_size_xy", "rotozoomSurfaceSizeXY (ROTO-001)", TEST_ENABLED};
static const SDLTest_TestCaseReference zoomRef = {test_zoom_identity_and_flip, "rotozoom_zoom_identity_flip", "Zoom identity, flips, 2x", TEST_ENABLED};
static const SDLTest_TestCaseReference rot90Ref = {test_rotate90, "rotozoom_rotate90", "rotateSurface90Degrees", TEST_ENABLED};
static const SDLTest_TestCaseReference rot90PalRef = {test_rotate90_palette, "rotozoom_rotate90_palette", "rotateSurface90Degrees keeps the palette (ROTO-005)", TEST_ENABLED};
static const SDLTest_TestCaseReference shrinkRef = {test_shrink, "rotozoom_shrink", "shrinkSurface", TEST_ENABLED};
static const SDLTest_TestCaseReference formatsRef = {test_formats, "rotozoom_formats", "8/24/32-bit handling", TEST_ENABLED};
static const SDLTest_TestCaseReference zoom8Ref = {test_zoom_8bit_flip, "rotozoom_zoom_8bit_flip", "8-bit zoom with flips (ROTO-002)", TEST_ENABLED};
static const SDLTest_TestCaseReference goldenXYRef = {test_golden_rotozoom_xy, "golden_rotozoom_xy", "Golden image: rotozoom XY (ROTO-001)", TEST_ENABLED};
static const SDLTest_TestCaseReference goldenRef = {test_golden_rotozoom, "golden_rotozoom", "Golden image: rotozoom", TEST_ENABLED};

static const SDLTest_TestCaseReference invalidRef = {test_invalid_factors, "rotozoom_invalid_factors", "NaN, infinite and huge factors (ROTO-004)", TEST_ENABLED};

static const SDLTest_TestCaseReference *rotozoomTests[] = {&sizeRef, &sizeXYRef, &zoomRef, &rot90Ref, &rot90PalRef, &shrinkRef, &formatsRef, &zoom8Ref, &goldenRef, &goldenXYRef, &invalidRef, NULL};

SDLTest_TestSuiteReference rotozoomTestSuite = {"Rotozoom", NULL, rotozoomTests, NULL};
