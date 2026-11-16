/*
 * Primitives on a hardware renderer (PERF-002).
 *
 * Draws scenes into a target texture of an accelerated renderer (hidden window) and checks that
 * - anti-aliased pixels drawn as batched SDL_RenderGeometry quads are bit-identical to drawing
 *   them as individual points (SDL3_GFX_AA_GEOMETRY hint), and
 * - the result matches the software renderer within a small tolerance (GPU blending rounds
 *   differently), which catches coordinate or coverage errors of the batched draw calls.
 *
 * The tests are skipped when no hardware renderer is available (e.g. headless CI).
 */
#include "testutils.h"

#include "SDL3_gfxPrimitives.h"

#define GPU_W         64
#define GPU_H         64
#define GPU_TOLERANCE 2

typedef void (*GpuScene)(SDL_Renderer *r);

static void scene_aa(SDL_Renderer *r)
{
	static const Sint16 px[] = {5, 58, 40, 12};
	static const Sint16 py[] = {8, 4, 50, 60};
	aalineRGBA(r, 2, 2, 61, 30, 255, 255, 255, 255);
	aalineRGBA(r, 2, 60, 30, 3, 255, 0, 0, 160);
	aalineRGBA(r, 10, 10, 10, 50, 0, 255, 0, 200);
	aalineRGBA(r, 20, 40, 50, 40, 0, 0, 255, 128);
	aalineRGBA(r, 5, 5, 25, 25, 255, 255, 0, 255);
	aacircleRGBA(r, 32, 32, 20, 0, 255, 255, 255);
	aaellipseRGBA(r, 32, 32, 28, 9, 255, 0, 255, 180);
	aapolygonRGBA(r, px, py, 4, 255, 128, 0, 220);
	aatrigonRGBA(r, 8, 40, 30, 62, 60, 45, 128, 255, 128, 140);
}

static void scene_solid(SDL_Renderer *r)
{
	static const Sint16 px[] = {4, 60, 50, 30, 8};
	static const Sint16 py[] = {4, 10, 60, 30, 50};
	filledPolygonRGBA(r, px, py, 5, 255, 0, 0, 255);
	filledPolygonRGBA(r, px, py, 5, 0, 255, 0, 100);
	filledCircleRGBA(r, 20, 44, 14, 0, 0, 255, 160);
	filledEllipseRGBA(r, 44, 20, 16, 8, 255, 255, 0, 200);
	circleRGBA(r, 32, 32, 25, 255, 255, 255, 255);
	ellipseRGBA(r, 32, 32, 30, 12, 0, 255, 255, 128);
	arcRGBA(r, 32, 32, 18, 30, 250, 255, 0, 255, 255);
	roundedBoxRGBA(r, 40, 40, 62, 62, 6, 255, 128, 0, 150);
	hlineRGBA(r, 3, 3, 62, 255, 255, 255, 128);
	vlineRGBA(r, 62, 3, 3, 255, 255, 255, 128);
	filledTrigonRGBA(r, 2, 62, 20, 50, 12, 40, 128, 128, 128, 128);
	thickLineRGBA(r, 5, 20, 58, 50, 4, 200, 0, 200, 160);
}

typedef struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *target;
} GpuContext;

static void gpu_close(GpuContext *ctx)
{
	if (ctx->target) {
		SDL_DestroyTexture(ctx->target);
	}
	if (ctx->renderer) {
		/* the glyph cache is not used here, but keep the documented order */
		gfxPrimitivesSetFont(NULL, 0, 0);
		SDL_DestroyRenderer(ctx->renderer);
	}
	if (ctx->window) {
		SDL_DestroyWindow(ctx->window);
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

/* Returns 0 if the named render driver is a hardware renderer with target texture support. */
static int gpu_open(GpuContext *ctx, const char *driver)
{
	SDL_zerop(ctx);
	if (SDL_strcmp(driver, SDL_SOFTWARE_RENDERER) == 0) {
		return -1;
	}
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
		SDLTest_Log("No video: %s", SDL_GetError());
		return -1;
	}
	ctx->window = SDL_CreateWindow("gfxtests", GPU_W, GPU_H, SDL_WINDOW_HIDDEN);
	if (ctx->window) {
		ctx->renderer = SDL_CreateRenderer(ctx->window, driver);
	}
	if (ctx->renderer) {
		ctx->target = SDL_CreateTexture(ctx->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, GPU_W, GPU_H);
	}
	if (ctx->target == NULL || !SDL_SetRenderTarget(ctx->renderer, ctx->target)) {
		gpu_close(ctx);
		return -1;
	}
	return 0;
}

static SDL_Surface *gpu_render(GpuContext *ctx, GpuScene scene, const char *aaGeometry)
{
	SDL_Surface *pixels, *s;
	SDL_SetHint("SDL3_GFX_AA_GEOMETRY", aaGeometry);
	SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_NONE);
	SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
	SDL_RenderClear(ctx->renderer);
	scene(ctx->renderer);
	pixels = SDL_RenderReadPixels(ctx->renderer, NULL);
	SDL_SetHint("SDL3_GFX_AA_GEOMETRY", NULL);
	if (pixels == NULL) {
		return NULL;
	}
	s = SDL_ConvertSurface(pixels, SDL_PIXELFORMAT_ARGB8888);
	SDL_DestroySurface(pixels);
	return s;
}

static SDL_Surface *software_render(GpuScene scene)
{
	GfxTestCanvas c;
	SDL_Surface *s;
	if (GfxTest_CreateCanvas(&c, GPU_W, GPU_H)) {
		return NULL;
	}
	scene(c.renderer);
	SDL_FlushRenderer(c.renderer);
	s = SDL_DuplicateSurface(c.surface);
	GfxTest_DestroyCanvas(&c);
	return s;
}

/* Counts pixels whose channels differ by more than tolerance. */
static int count_differences(SDL_Surface *a, SDL_Surface *b, int tolerance, int *maxdiff)
{
	int x, y, i, n = 0;
	*maxdiff = 0;
	for (y = 0; y < GPU_H; y++) {
		const Uint8 *pa = (const Uint8 *)a->pixels + y * a->pitch;
		const Uint8 *pb = (const Uint8 *)b->pixels + y * b->pitch;
		for (x = 0; x < GPU_W; x++) {
			int bad = 0;
			for (i = 0; i < 3; i++) {
				const int d = SDL_abs(pa[x * 4 + i] - pb[x * 4 + i]);
				*maxdiff = SDL_max(*maxdiff, d);
				bad |= d > tolerance;
			}
			n += bad;
		}
	}
	return n;
}

static int run_scene(GpuScene scene, const char *name)
{
	GpuContext ctx;
	const char *driver;
	SDL_Surface *points, *geometry, *software;
	int i, diff, maxdiff, tested = 0;

	software = software_render(scene);
	if (software == NULL) {
		return TEST_ABORTED;
	}
	for (i = 0; i < SDL_GetNumRenderDrivers(); i++) {
		driver = SDL_GetRenderDriver(i);
		if (driver == NULL || gpu_open(&ctx, driver)) {
			continue;
		}
		tested++;
		points = gpu_render(&ctx, scene, "0");
		geometry = gpu_render(&ctx, scene, "1");
		if (points && geometry) {
			diff = count_differences(points, geometry, 0, &maxdiff);
			SDLTest_AssertCheck(diff == 0, "%s/%s: AA geometry equals AA points (%d pixels differ, max %d)", name, driver, diff, maxdiff);
			diff = count_differences(geometry, software, GPU_TOLERANCE, &maxdiff);
			SDLTest_AssertCheck(diff == 0, "%s/%s: hardware equals software within %d (%d pixels differ, max %d)", name, driver,
				GPU_TOLERANCE, diff, maxdiff);
			if (diff) {
				char path[512], file[128];
				SDL_snprintf(file, sizeof(file), "gpu_%s_%s.bmp", name, driver);
				SDL_SaveBMP(geometry, GfxTest_Path(path, sizeof(path), gfxtest_options.output_dir, file));
				SDL_snprintf(file, sizeof(file), "gpu_%s_software.bmp", name);
				SDL_SaveBMP(software, GfxTest_Path(path, sizeof(path), gfxtest_options.output_dir, file));
			}
		} else {
			SDLTest_AssertCheck(0, "%s/%s: rendering failed: %s", name, driver, SDL_GetError());
		}
		SDL_DestroySurface(points);
		SDL_DestroySurface(geometry);
		gpu_close(&ctx);
	}
	SDL_DestroySurface(software);
	if (!tested) {
		SDLTest_Log("No hardware renderer with target textures available");
		return TEST_SKIPPED;
	}
	return TEST_COMPLETED;
}
static int gpu_antialiased(void *arg)
{
	(void)arg;
	return run_scene(scene_aa, "anti-aliased");
}

static int gpu_solid(void *arg)
{
	(void)arg;
	return run_scene(scene_solid, "solid");
}

static const SDLTest_TestCaseReference gpuTest1 = {gpu_antialiased, "gpu_antialiased", "AA primitives on a hardware renderer", TEST_ENABLED};
static const SDLTest_TestCaseReference gpuTest2 = {gpu_solid, "gpu_solid", "Batched solid primitives on a hardware renderer", TEST_ENABLED};

static const SDLTest_TestCaseReference *gpuTests[] = {&gpuTest1, &gpuTest2, NULL};

SDLTest_TestSuiteReference primitivesGpuTestSuite = {"PrimitivesGPU", NULL, gpuTests, NULL};
