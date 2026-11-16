/*
 * SDL3_gfx benchmark: throughput of the drawing primitives, rotozoomer and image filters.
 *
 *   gfxbench [--renderer software|<SDL render driver>] [--time <seconds per item>] [--csv <file>]
 *            [--filter <substring>]
 *
 * "software" (default) draws into a 640x480 surface with SDL's software renderer (deterministic,
 * no window). Any other value creates a hidden 640x480 window with that SDL render driver (e.g.
 * direct3d11, direct3d12, opengl, vulkan) and synchronizes with SDL_RenderReadPixels after each item.
 * Results: operations per second (primitives, rotozoom) or MB/s (image filters).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "SDL3_gfxPrimitives.h"
#include "SDL3_imageFilter.h"
#include "SDL3_rotozoom.h"

#define W 640
#define H 480

static SDL_Renderer *renderer;
static double seconds_per_item = 0.25;
static const char *filter;
static FILE *csv;
static const char *renderer_name = "software";
static Uint32 rng_state = 0x12345678u;

static Uint32 rnd(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static Sint16 rx(void)
{ return (Sint16)(rnd() % W); }
static Sint16 ry(void)
{ return (Sint16)(rnd() % H); }
static Uint8 rc(void)
{ return (Uint8)rnd(); }

static void sync_renderer(void)
{
	SDL_Rect r = {0, 0, 1, 1};
	SDL_FlushRenderer(renderer);
	SDL_DestroySurface(SDL_RenderReadPixels(renderer, &r));
}

static void report(const char *group, const char *name, double value, const char *unit, Uint64 count)
{
	printf("%-12s %-28s %14.1f %s\n", group, name, value, unit);
	if (csv) {
		fprintf(csv, "%s,%s,\"%s\",%.3f,%s,%llu\n", renderer_name, group, name, value, unit, (unsigned long long)count);
	}
}

typedef void (*DrawFn)(Uint8 alpha);

static void run_primitive(const char *name, DrawFn fn, Uint8 alpha)
{
	char label[64];
	Uint64 freq = SDL_GetPerformanceFrequency(), start, now, count = 0;
	SDL_snprintf(label, sizeof(label), "%s%s", name, alpha == 255 ? "" : " (blend)");
	if (filter && !SDL_strstr(label, filter)) {
		return;
	}
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	rng_state = 0x12345678u;
	start = SDL_GetPerformanceCounter();
	do {
		int i;
		for (i = 0; i < 16; i++) {
			fn(alpha);
		}
		count += 16;
		if ((count & 255) == 0) {
			sync_renderer();
		}
		now = SDL_GetPerformanceCounter();
	} while ((double)(now - start) / (double)freq < seconds_per_item);
	sync_renderer();
	now = SDL_GetPerformanceCounter();
	report("primitive", label, (double)count * (double)freq / (double)(now - start), "ops/s", count);
}

static void d_pixel(Uint8 a)
{ pixelRGBA(renderer, rx(), ry(), rc(), rc(), rc(), a); }
static void d_hline(Uint8 a)
{ hlineRGBA(renderer, rx(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_vline(Uint8 a)
{ vlineRGBA(renderer, rx(), ry(), ry(), rc(), rc(), rc(), a); }
static void d_rectangle(Uint8 a)
{ rectangleRGBA(renderer, rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_rounded_rectangle(Uint8 a)
{ roundedRectangleRGBA(renderer, rx(), ry(), rx(), ry(), 8, rc(), rc(), rc(), a); }
static void d_box(Uint8 a)
{ boxRGBA(renderer, rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_rounded_box(Uint8 a)
{ roundedBoxRGBA(renderer, rx(), ry(), rx(), ry(), 8, rc(), rc(), rc(), a); }
static void d_line(Uint8 a)
{ lineRGBA(renderer, rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_aaline(Uint8 a)
{ aalineRGBA(renderer, rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_thick_line(Uint8 a)
{ thickLineRGBA(renderer, rx(), ry(), rx(), ry(), 5, rc(), rc(), rc(), a); }
static void d_circle(Uint8 a)
{ circleRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_aacircle(Uint8 a)
{ aacircleRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_filled_circle(Uint8 a)
{ filledCircleRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_arc(Uint8 a)
{ arcRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 360), (Sint16)(rnd() % 360), rc(), rc(), rc(), a); }
static void d_ellipse(Uint8 a)
{ ellipseRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_aaellipse(Uint8 a)
{ aaellipseRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_filled_ellipse(Uint8 a)
{ filledEllipseRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 100), rc(), rc(), rc(), a); }
static void d_pie(Uint8 a)
{ pieRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 360), (Sint16)(rnd() % 360), rc(), rc(), rc(), a); }
static void d_filled_pie(Uint8 a)
{ filledPieRGBA(renderer, rx(), ry(), (Sint16)(rnd() % 100), (Sint16)(rnd() % 360), (Sint16)(rnd() % 360), rc(), rc(), rc(), a); }
static void d_trigon(Uint8 a)
{ trigonRGBA(renderer, rx(), ry(), rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_aatrigon(Uint8 a)
{ aatrigonRGBA(renderer, rx(), ry(), rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }
static void d_filled_trigon(Uint8 a)
{ filledTrigonRGBA(renderer, rx(), ry(), rx(), ry(), rx(), ry(), rc(), rc(), rc(), a); }

static void random_polygon(Sint16 *vx, Sint16 *vy, int n)
{
	int i;
	Sint16 cx = rx(), cy = ry();
	for (i = 0; i < n; i++) {
		vx[i] = (Sint16)(cx + (Sint16)(rnd() % 160) - 80);
		vy[i] = (Sint16)(cy + (Sint16)(rnd() % 160) - 80);
	}
}

static void d_polygon(Uint8 a)
{
	Sint16 vx[8], vy[8];
	random_polygon(vx, vy, 8);
	polygonRGBA(renderer, vx, vy, 8, rc(), rc(), rc(), a);
}
static void d_aapolygon(Uint8 a)
{
	Sint16 vx[8], vy[8];
	random_polygon(vx, vy, 8);
	aapolygonRGBA(renderer, vx, vy, 8, rc(), rc(), rc(), a);
}
static void d_filled_polygon(Uint8 a)
{
	Sint16 vx[8], vy[8];
	random_polygon(vx, vy, 8);
	filledPolygonRGBA(renderer, vx, vy, 8, rc(), rc(), rc(), a);
}
static void d_bezier(Uint8 a)
{
	Sint16 vx[4], vy[4];
	random_polygon(vx, vy, 4);
	bezierRGBA(renderer, vx, vy, 4, 32, rc(), rc(), rc(), a);
}
static void d_string(Uint8 a)
{ stringRGBA(renderer, rx(), ry(), "The quick brown fox", rc(), rc(), rc(), a); }

static SDL_Surface *texture;
static void d_textured_polygon(Uint8 a)
{
	Sint16 vx[8], vy[8];
	(void)a;
	random_polygon(vx, vy, 8);
	texturedPolygon(renderer, vx, vy, 8, texture, 0, 0);
}

static void bench_primitives(void)
{
	static const struct {
		const char *name;
		DrawFn fn;
		int blend;
	} items[] = {
		{"pixel", d_pixel, 1},
		{"hline", d_hline, 1},
		{"vline", d_vline, 1},
		{"rectangle", d_rectangle, 1},
		{"roundedRectangle", d_rounded_rectangle, 1},
		{"box", d_box, 1},
		{"roundedBox", d_rounded_box, 1},
		{"line", d_line, 1},
		{"aaline", d_aaline, 1},
		{"thickLine(5)", d_thick_line, 1},
		{"circle", d_circle, 1},
		{"aacircle", d_aacircle, 1},
		{"filledCircle", d_filled_circle, 1},
		{"arc", d_arc, 1},
		{"ellipse", d_ellipse, 1},
		{"aaellipse", d_aaellipse, 1},
		{"filledEllipse", d_filled_ellipse, 1},
		{"pie", d_pie, 1},
		{"filledPie", d_filled_pie, 1},
		{"trigon", d_trigon, 1},
		{"aatrigon", d_aatrigon, 1},
		{"filledTrigon", d_filled_trigon, 1},
		{"polygon(8)", d_polygon, 1},
		{"aapolygon(8)", d_aapolygon, 1},
		{"filledPolygon(8)", d_filled_polygon, 1},
		{"texturedPolygon(8)", d_textured_polygon, 0},
		{"bezier(4,32)", d_bezier, 1},
		{"string(19)", d_string, 1},
	};
	size_t i;
	texture = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
	SDL_FillSurfaceRect(texture, NULL, 0xFF80C040);
	for (i = 0; i < SDL_arraysize(items); i++) {
		run_primitive(items[i].name, items[i].fn, 255);
		if (items[i].blend) {
			run_primitive(items[i].name, items[i].fn, 128);
		}
	}
	SDL_DestroySurface(texture);
}

static void bench_rotozoom(void)
{
	static const struct {
		const char *name;
		double angle, zoom;
		int smooth;
	} items[] = {
		{"rotozoom 30deg x1", 30.0, 1.0, SMOOTHING_OFF},
		{"rotozoom 30deg x1 smooth", 30.0, 1.0, SMOOTHING_ON},
		{"zoom x2", 0.0, 2.0, SMOOTHING_OFF},
		{"zoom x2 smooth", 0.0, 2.0, SMOOTHING_ON},
		{"zoom x0.5 smooth", 0.0, 0.5, SMOOTHING_ON},
	};
	SDL_Surface *src = SDL_CreateSurface(256, 256, SDL_PIXELFORMAT_ABGR8888);
	size_t i;
	int x;
	for (x = 0; x < 256 * 256; x++) {
		((Uint32 *)src->pixels)[x] = rnd() | 0xFF000000u;
	}
	for (i = 0; i < SDL_arraysize(items); i++) {
		Uint64 freq = SDL_GetPerformanceFrequency(), start = SDL_GetPerformanceCounter(), now, count = 0;
		if (filter && !SDL_strstr(items[i].name, filter)) {
			continue;
		}
		do {
			SDL_DestroySurface(rotozoomSurface(src, items[i].angle, items[i].zoom, items[i].smooth));
			count++;
			now = SDL_GetPerformanceCounter();
		} while ((double)(now - start) / (double)freq < seconds_per_item);
		report("rotozoom", items[i].name, (double)count * (double)freq / (double)(now - start), "ops/s(256^2)", count);
	}
	SDL_DestroySurface(src);
}

static void bench_imagefilter(void)
{
	const unsigned int len = 1024 * 1024;
	unsigned char *a = (unsigned char *)malloc(len), *b = (unsigned char *)malloc(len), *d = (unsigned char *)malloc(len);
	unsigned int i;
	int k;
	static const char *names[] = {"Add", "Mean", "AbsDiff", "Mult", "AddByte", "ShiftRight", "BinarizeUsingThreshold", "NormalizeLinear"};
	if (!a || !b || !d) {
		free(a);
		free(b);
		free(d);
		return;
	}
	for (i = 0; i < len; i++) {
		a[i] = (unsigned char)rnd();
		b[i] = (unsigned char)rnd();
	}
	for (k = 0; k < (int)SDL_arraysize(names); k++) {
		Uint64 freq = SDL_GetPerformanceFrequency(), start = SDL_GetPerformanceCounter(), now, count = 0;
		if (filter && !SDL_strstr(names[k], filter)) {
			continue;
		}
		do {
			switch (k) {
				case 0:
					SDL_imageFilterAdd(a, b, d, len);
					break;
				case 1:
					SDL_imageFilterMean(a, b, d, len);
					break;
				case 2:
					SDL_imageFilterAbsDiff(a, b, d, len);
					break;
				case 3:
					SDL_imageFilterMult(a, b, d, len);
					break;
				case 4:
					SDL_imageFilterAddByte(a, d, len, 17);
					break;
				case 5:
					SDL_imageFilterShiftRight(a, d, len, 2);
					break;
				case 6:
					SDL_imageFilterBinarizeUsingThreshold(a, d, len, 128);
					break;
				default:
					SDL_imageFilterNormalizeLinear(a, d, len, 0, 255, 50, 200);
					break;
			}
			count++;
			now = SDL_GetPerformanceCounter();
		} while ((double)(now - start) / (double)freq < seconds_per_item);
		report("imagefilter", names[k], (double)count * (double)freq / (double)(now - start), "MB/s", count);
	}
	free(a);
	free(b);
	free(d);
}

int main(int argc, char *argv[])
{
	SDL_Window *window = NULL;
	SDL_Surface *surface = NULL;
	const char *csv_path = NULL;
	int i;

	for (i = 1; i < argc; i++) {
		if (i + 1 < argc && SDL_strcmp(argv[i], "--renderer") == 0) {
			renderer_name = argv[++i];
		} else if (i + 1 < argc && SDL_strcmp(argv[i], "--time") == 0) {
			seconds_per_item = SDL_atof(argv[++i]);
		} else if (i + 1 < argc && SDL_strcmp(argv[i], "--csv") == 0) {
			csv_path = argv[++i];
		} else if (i + 1 < argc && SDL_strcmp(argv[i], "--filter") == 0) {
			filter = argv[++i];
		} else {
			printf("Usage: %s [--renderer software|<driver>] [--time <s>] [--csv <file>] [--filter <text>]\n", argv[0]);
			return 2;
		}
	}

	if (SDL_strcmp(renderer_name, "software") == 0) {
		SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
	}
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	if (SDL_strcmp(renderer_name, "software") == 0) {
		surface = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
		renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
	} else {
		window = SDL_CreateWindow("gfxbench", W, H, SDL_WINDOW_HIDDEN);
		renderer = window ? SDL_CreateRenderer(window, renderer_name) : NULL;
	}
	if (renderer == NULL) {
		fprintf(stderr, "Cannot create renderer '%s': %s\n", renderer_name, SDL_GetError());
		SDL_Quit();
		return 1;
	}
	if (csv_path) {
		csv = fopen(csv_path, "w");
		if (csv) {
			fprintf(csv, "renderer,group,item,value,unit,count\n");
		}
	}
	printf("SDL3_gfx %d.%d.%d benchmark, renderer %s, %.2f s per item\n", SDL3_GFXPRIMITIVES_MAJOR, SDL3_GFXPRIMITIVES_MINOR,
		SDL3_GFXPRIMITIVES_MICRO, SDL_GetRendererName(renderer), seconds_per_item);

	bench_primitives();
	gfxPrimitivesSetFont(NULL, 0, 0); /* release cached glyph textures before the renderer */
	bench_rotozoom();
	bench_imagefilter();

	if (csv) {
		fclose(csv);
	}
	SDL_DestroyRenderer(renderer);
	SDL_DestroySurface(surface);
	if (window) {
		SDL_DestroyWindow(window);
	}
	SDL_Quit();
	return 0;
}
