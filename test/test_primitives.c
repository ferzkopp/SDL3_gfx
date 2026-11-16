/*
 * Golden-image and API tests for SDL3_gfxPrimitives.
 *
 * Every golden test draws a 64x64 scene that exercises one primitive family including edge cases
 * (reversed coordinates, degenerate sizes, clipping at the canvas border, alpha blending) and
 * compares it with test/golden/sdl3/<name>.bmp. The goldens capture the behavior of the code at the
 * time they were generated; intentional changes must reference a finding in docs/CODE_REVIEW.md.
 */
#include "testutils.h"

#include "SDL3_gfxPrimitives.h"

#define RED    255, 0, 0
#define GREEN  0, 255, 0
#define BLUE   0, 0, 255
#define WHITE  255, 255, 255
#define YELLOW 255, 255, 0

/* Runs a scene on a fresh canvas and compares it with the golden image. */
static int run_golden(const char *name, void (*scene)(SDL_Renderer *))
{
	GfxTestCanvas canvas;
	if (GfxTest_CreateCanvas(&canvas, GFXTEST_CANVAS_W, GFXTEST_CANVAS_H) != 0) {
		return TEST_ABORTED;
	}
	scene(canvas.renderer);
	GfxTest_CompareGolden(&canvas, name);
	GfxTest_DestroyCanvas(&canvas);
	return TEST_COMPLETED;
}

#define GOLDEN_TEST(name)                                        \
	static int golden_##name(void *arg)                          \
	{                                                            \
		(void)arg;                                               \
		return run_golden(#name, scene_##name);                  \
	}                                                            \
	static const SDLTest_TestCaseReference golden_##name##_ref = \
		{golden_##name, "golden_" #name, "Golden image: " #name, TEST_ENABLED};

/* --- Scenes ------------------------------------------------------------------------------- */

static void scene_pixel(SDL_Renderer *r)
{
	int i;
	for (i = 0; i < 16; i++) {
		pixelRGBA(r, (Sint16)(4 + i * 3), 4, RED, 255);
		pixelRGBA(r, (Sint16)(4 + i * 3), 8, GREEN, (Uint8)(i * 16 + 15));
		pixelColor(r, (Sint16)(4 + i * 3), 12, 0x0000FFFF);
	}
	/* off-canvas pixels must be ignored */
	pixelRGBA(r, -1, -1, WHITE, 255);
	pixelRGBA(r, 64, 64, WHITE, 255);
	pixelRGBA(r, -32768, 32767, WHITE, 255);
	/* blending on a non-black background */
	boxRGBA(r, 8, 20, 55, 40, 40, 40, 160, 255);
	for (i = 0; i < 16; i++) {
		pixelRGBA(r, (Sint16)(10 + i * 3), 30, YELLOW, (Uint8)(i * 16 + 15));
	}
}

static void scene_hline_vline(SDL_Renderer *r)
{
	hlineRGBA(r, 2, 61, 2, RED, 255);
	hlineRGBA(r, 61, 2, 5, GREEN, 255); /* reversed */
	hlineRGBA(r, 10, 10, 8, WHITE, 255); /* single pixel */
	hlineRGBA(r, -20, 100, 11, BLUE, 255); /* clipped both sides */
	hlineRGBA(r, -20, -5, 14, WHITE, 255); /* fully outside */
	hlineRGBA(r, 0, 63, 70, WHITE, 255); /* below canvas */
	hlineColor(r, 5, 40, 17, 0xFFFF00FF);
	vlineRGBA(r, 20, 20, 61, RED, 255);
	vlineRGBA(r, 23, 61, 20, GREEN, 255); /* reversed */
	vlineRGBA(r, 26, -10, 100, BLUE, 255); /* clipped */
	vlineColor(r, 29, 25, 45, 0x00FFFFFF);
	/* overlapping semi-transparent lines */
	hlineRGBA(r, 30, 60, 40, WHITE, 128);
	vlineRGBA(r, 45, 30, 60, WHITE, 128);
}

static void scene_rectangle(SDL_Renderer *r)
{
	rectangleRGBA(r, 2, 2, 30, 20, RED, 255);
	rectangleRGBA(r, 60, 20, 34, 2, GREEN, 255); /* reversed */
	rectangleRGBA(r, 5, 25, 5, 40, WHITE, 255); /* degenerate: vertical line */
	rectangleRGBA(r, 10, 30, 25, 30, WHITE, 255); /* degenerate: horizontal line */
	rectangleRGBA(r, 12, 34, 12, 34, YELLOW, 255); /* degenerate: point */
	rectangleRGBA(r, -10, 45, 20, 80, BLUE, 255); /* clipped */
	rectangleColor(r, 30, 30, 60, 60, 0xFF00FFFF);
	rectangleRGBA(r, 35, 35, 55, 55, WHITE, 100);
}

static void scene_rounded_rectangle(SDL_Renderer *r)
{
	roundedRectangleRGBA(r, 2, 2, 30, 20, 5, RED, 255);
	roundedRectangleRGBA(r, 60, 20, 34, 2, 3, GREEN, 255); /* reversed */
	roundedRectangleRGBA(r, 2, 24, 30, 40, 0, WHITE, 255); /* rad 0 = rectangle */
	roundedRectangleRGBA(r, 34, 24, 60, 40, 50, YELLOW, 255); /* rad larger than size */
	roundedRectangleRGBA(r, -10, 44, 20, 80, 6, BLUE, 255); /* clipped */
	roundedRectangleColor(r, 30, 44, 60, 60, 4, 0xFF00FFFF);
	roundedRectangleRGBA(r, 40, 48, 40, 55, 2, WHITE, 255); /* zero width */
}

static void scene_box(SDL_Renderer *r)
{
	boxRGBA(r, 2, 2, 30, 20, RED, 255);
	boxRGBA(r, 60, 20, 34, 2, GREEN, 255); /* reversed */
	boxRGBA(r, 5, 25, 5, 40, WHITE, 255); /* degenerate */
	boxRGBA(r, -10, 45, 20, 80, BLUE, 255); /* clipped */
	boxColor(r, 30, 30, 60, 60, 0xFF00FFFF);
	boxRGBA(r, 20, 35, 50, 50, WHITE, 128); /* blended over red/magenta */
}

static void scene_rounded_box(SDL_Renderer *r)
{
	roundedBoxRGBA(r, 2, 2, 30, 20, 5, RED, 255);
	roundedBoxRGBA(r, 60, 20, 34, 2, 3, GREEN, 255); /* reversed */
	roundedBoxRGBA(r, 2, 24, 30, 40, 0, WHITE, 255); /* rad 0 = box */
	roundedBoxRGBA(r, 34, 24, 60, 40, 50, YELLOW, 255); /* rad larger than size */
	roundedBoxRGBA(r, -10, 44, 20, 80, 6, BLUE, 255); /* clipped */
	roundedBoxColor(r, 30, 44, 60, 60, 4, 0xFF00FFFF);
	roundedBoxRGBA(r, 36, 48, 54, 58, 3, WHITE, 128);
}

static void star(SDL_Renderer *r, int aa)
{
	static const Sint16 ends[][2] = {{62, 32}, {62, 50}, {62, 62}, {50, 62}, {32, 62}, {14, 62}, {1, 62}, {1, 50},
		{1, 32}, {1, 14}, {1, 1}, {14, 1}, {32, 1}, {50, 1}, {62, 1}, {62, 14}};
	int i;
	for (i = 0; i < 16; i++) {
		Uint8 c = (Uint8)(80 + i * 10);
		if (aa) {
			aalineRGBA(r, 32, 32, ends[i][0], ends[i][1], c, 255, (Uint8)(255 - c), 255);
		} else {
			lineRGBA(r, 32, 32, ends[i][0], ends[i][1], c, 255, (Uint8)(255 - c), 255);
		}
	}
}

static void scene_line(SDL_Renderer *r)
{
	star(r, 0);
	lineRGBA(r, -50, 10, 100, 20, RED, 255); /* clipped */
	lineRGBA(r, 10, 10, 10, 10, WHITE, 255); /* single point */
	lineColor(r, 5, 55, 25, 45, 0xFFFF00FF);
	lineRGBA(r, 40, 58, 60, 40, WHITE, 128);
}

static void scene_aaline(SDL_Renderer *r)
{
	star(r, 1);
	aalineRGBA(r, -50, 10, 100, 20, RED, 255); /* clipped */
	aalineRGBA(r, 10, 10, 10, 10, WHITE, 255); /* single point */
	aalineColor(r, 5, 55, 25, 45, 0xFFFF00FF);
	aalineRGBA(r, 40, 58, 60, 40, WHITE, 128);
}

static void scene_thick_line(SDL_Renderer *r)
{
	thickLineRGBA(r, 4, 4, 60, 4, 1, RED, 255);
	thickLineRGBA(r, 4, 10, 60, 10, 2, GREEN, 255);
	thickLineRGBA(r, 4, 18, 60, 18, 5, BLUE, 255);
	thickLineRGBA(r, 6, 26, 6, 60, 4, WHITE, 255); /* vertical */
	thickLineRGBA(r, 12, 28, 58, 60, 7, YELLOW, 255); /* diagonal */
	thickLineRGBA(r, 58, 28, 30, 44, 3, 255, 0, 255, 160); /* blended */
	thickLineRGBA(r, 20, 50, 20, 50, 6, WHITE, 255); /* zero length */
	thickLineColor(r, -20, 62, 80, 40, 3, 0x00FFFFFF); /* clipped */
}

static void scene_circle(SDL_Renderer *r)
{
	circleRGBA(r, 32, 32, 28, RED, 255);
	circleRGBA(r, 32, 32, 20, GREEN, 255);
	circleRGBA(r, 10, 10, 0, WHITE, 255); /* rad 0 = pixel */
	circleRGBA(r, 16, 10, 1, WHITE, 255);
	circleRGBA(r, 24, 10, 2, WHITE, 255);
	circleRGBA(r, 0, 63, 12, BLUE, 255); /* clipped */
	circleColor(r, 40, 40, 8, 0xFFFF00FF);
	circleRGBA(r, 32, 32, 12, WHITE, 128);
}

static void scene_aacircle(SDL_Renderer *r)
{
	aacircleRGBA(r, 32, 32, 28, RED, 255);
	aacircleRGBA(r, 32, 32, 20, GREEN, 255);
	aacircleRGBA(r, 10, 10, 0, WHITE, 255);
	aacircleRGBA(r, 16, 10, 1, WHITE, 255);
	aacircleRGBA(r, 24, 10, 2, WHITE, 255);
	aacircleRGBA(r, 0, 63, 12, BLUE, 255); /* clipped */
	aacircleColor(r, 40, 40, 8, 0xFFFF00FF);
	aacircleRGBA(r, 32, 32, 12, WHITE, 128);
}

static void scene_filled_circle(SDL_Renderer *r)
{
	filledCircleRGBA(r, 20, 20, 16, RED, 255);
	filledCircleRGBA(r, 44, 20, 1, WHITE, 255);
	filledCircleRGBA(r, 50, 20, 0, WHITE, 255);
	filledCircleRGBA(r, 0, 63, 14, BLUE, 255); /* clipped */
	filledCircleColor(r, 44, 44, 14, 0x00FF00FF);
	filledCircleRGBA(r, 34, 34, 12, WHITE, 128); /* blended overlap */
}

static void scene_arc(SDL_Renderer *r)
{
	arcRGBA(r, 16, 16, 12, 0, 90, RED, 255);
	arcRGBA(r, 48, 16, 12, 90, 0, GREEN, 255); /* wrap-around */
	arcRGBA(r, 16, 48, 12, -45, 45, BLUE, 255); /* negative start */
	arcRGBA(r, 48, 48, 12, 400, 500, YELLOW, 255); /* angles > 360 */
	arcRGBA(r, 48, 48, 6, 0, 360, WHITE, 255); /* full circle */
	arcRGBA(r, 16, 48, 4, 30, 30, WHITE, 255); /* empty range */
	arcColor(r, 32, 32, 30, 200, 340, 0xFF00FFFF);
}

static void scene_ellipse(SDL_Renderer *r)
{
	ellipseRGBA(r, 32, 16, 28, 12, RED, 255);
	ellipseRGBA(r, 16, 44, 10, 18, GREEN, 255);
	ellipseRGBA(r, 40, 40, 0, 10, WHITE, 255); /* rx 0 */
	ellipseRGBA(r, 50, 40, 10, 0, WHITE, 255); /* ry 0 */
	ellipseRGBA(r, 63, 63, 20, 8, BLUE, 255); /* clipped */
	ellipseColor(r, 48, 50, 8, 4, 0xFFFF00FF);
}

static void scene_aaellipse(SDL_Renderer *r)
{
	aaellipseRGBA(r, 32, 16, 28, 12, RED, 255);
	aaellipseRGBA(r, 16, 44, 10, 18, GREEN, 255);
	aaellipseRGBA(r, 40, 40, 0, 10, WHITE, 255);
	aaellipseRGBA(r, 50, 40, 10, 0, WHITE, 255);
	aaellipseRGBA(r, 63, 63, 20, 8, BLUE, 255);
	aaellipseColor(r, 48, 50, 8, 4, 0xFFFF00FF);
}

static void scene_filled_ellipse(SDL_Renderer *r)
{
	filledEllipseRGBA(r, 32, 16, 28, 12, RED, 255);
	filledEllipseRGBA(r, 16, 44, 10, 18, GREEN, 255);
	filledEllipseRGBA(r, 63, 63, 20, 8, BLUE, 255);
	filledEllipseColor(r, 44, 46, 12, 6, 0xFFFF00FF);
	filledEllipseRGBA(r, 32, 32, 14, 20, WHITE, 100);
}

static void scene_pie(SDL_Renderer *r)
{
	pieRGBA(r, 16, 16, 13, 0, 90, RED, 255);
	pieRGBA(r, 48, 16, 13, 90, 0, GREEN, 255);
	pieRGBA(r, 16, 48, 13, -45, 45, BLUE, 255);
	pieRGBA(r, 48, 48, 13, 400, 500, YELLOW, 255);
	pieColor(r, 32, 32, 6, 0, 359, 0xFFFFFFFF);
}

static void scene_filled_pie(SDL_Renderer *r)
{
	filledPieRGBA(r, 16, 16, 13, 0, 90, RED, 255);
	filledPieRGBA(r, 48, 16, 13, 90, 0, GREEN, 255);
	filledPieRGBA(r, 16, 48, 13, -45, 45, BLUE, 255);
	filledPieRGBA(r, 48, 48, 13, 400, 500, YELLOW, 255);
	filledPieColor(r, 32, 32, 10, 30, 300, 0xFFFFFF80);
}

static void scene_trigon(SDL_Renderer *r)
{
	trigonRGBA(r, 2, 2, 30, 2, 16, 28, RED, 255);
	aatrigonRGBA(r, 34, 2, 62, 2, 48, 28, GREEN, 255);
	filledTrigonRGBA(r, 2, 34, 30, 34, 16, 62, BLUE, 255);
	filledTrigonRGBA(r, 34, 62, 62, 62, 48, 34, YELLOW, 255); /* flipped winding */
	filledTrigonRGBA(r, 40, 40, 50, 50, 60, 60, WHITE, 255); /* collinear */
	trigonColor(r, -10, 30, 70, 30, 32, 70, 0xFF00FFFF); /* clipped */
	aatrigonColor(r, 10, 10, 20, 10, 15, 20, 0x00FFFFFF);
	filledTrigonColor(r, 20, 40, 44, 40, 32, 60, 0xFFFFFF80);
}

static const Sint16 concave_x[] = {4, 28, 16, 28, 4};
static const Sint16 concave_y[] = {4, 4, 16, 28, 28};
static const Sint16 star_x[] = {48, 52, 62, 54, 58, 48, 38, 42, 34, 44};
static const Sint16 star_y[] = {2, 12, 12, 18, 28, 22, 28, 18, 12, 12};
static const Sint16 bowtie_x[] = {4, 28, 4, 28};
static const Sint16 bowtie_y[] = {36, 60, 60, 36};
static const Sint16 clip_x[] = {40, 80, 50};
static const Sint16 clip_y[] = {40, 50, 80};

static void scene_polygon(SDL_Renderer *r)
{
	polygonRGBA(r, concave_x, concave_y, 5, RED, 255);
	polygonRGBA(r, star_x, star_y, 10, GREEN, 255);
	polygonRGBA(r, bowtie_x, bowtie_y, 4, BLUE, 255);
	polygonColor(r, clip_x, clip_y, 3, 0xFFFF00FF);
}

static void scene_aapolygon(SDL_Renderer *r)
{
	aapolygonRGBA(r, concave_x, concave_y, 5, RED, 255);
	aapolygonRGBA(r, star_x, star_y, 10, GREEN, 255);
	aapolygonRGBA(r, bowtie_x, bowtie_y, 4, BLUE, 255);
	aapolygonColor(r, clip_x, clip_y, 3, 0xFFFF00FF);
}

static void scene_filled_polygon(SDL_Renderer *r)
{
	filledPolygonRGBA(r, concave_x, concave_y, 5, RED, 255);
	filledPolygonRGBA(r, star_x, star_y, 10, GREEN, 255);
	filledPolygonRGBA(r, bowtie_x, bowtie_y, 4, BLUE, 255);
	filledPolygonColor(r, clip_x, clip_y, 3, 0xFFFF00FF);
	filledPolygonRGBA(r, star_x, star_y, 10, WHITE, 100); /* blended over itself */
}

static void scene_textured_polygon(SDL_Renderer *r)
{
	SDL_Surface *texture = GfxTest_LoadData("sample24.bmp");
	if (texture == NULL) {
		return;
	}
	texturedPolygon(r, concave_x, concave_y, 5, texture, 0, 0);
	texturedPolygon(r, star_x, star_y, 10, texture, 10, 20);
	texturedPolygon(r, bowtie_x, bowtie_y, 4, texture, -5, -7);
	texturedPolygon(r, clip_x, clip_y, 3, texture, 0, 0);
	SDL_DestroySurface(texture);
}

static void scene_bezier(SDL_Renderer *r)
{
	static const Sint16 bx[] = {2, 20, 44, 62};
	static const Sint16 by[] = {30, 2, 62, 30};
	static const Sint16 cx[] = {2, 32, 62};
	static const Sint16 cy[] = {62, 20, 62};
	bezierRGBA(r, bx, by, 4, 2, RED, 255);
	bezierRGBA(r, bx, by, 4, 20, GREEN, 255);
	bezierColor(r, cx, cy, 3, 10, 0x0000FFFF);
}

static void scene_text(SDL_Renderer *r)
{
	/* Same text as in SDL2_gfx: the golden images of both libraries are identical (parity test) */
	stringRGBA(r, 0, 0, "SDL2_gfx", WHITE, 255);
	stringRGBA(r, 0, 10, "!@#$%^&*", RED, 255);
	stringColor(r, 0, 20, "0123456789", 0x00FF00FF);
	characterRGBA(r, 0, 30, 'A', YELLOW, 255);
	characterRGBA(r, 10, 30, "\xE4"[0], YELLOW, 255); /* character > 127 */
	stringRGBA(r, -4, 40, "clip", BLUE, 255); /* clipped at the left border */
	stringRGBA(r, 40, 40, "clip", BLUE, 255); /* clipped at the right border */
	stringRGBA(r, 0, 52, "blend", WHITE, 128);
	gfxPrimitivesSetFontRotation(1);
	stringRGBA(r, 54, 10, "rot", 255, 0, 255, 255);
	gfxPrimitivesSetFontRotation(0);
}

static void scene_blend_overlap(SDL_Renderer *r)
{
	/* Semi-transparent outlines and fills overlapping themselves: captures whether pixels are
	   blended more than once (e.g. at octant seams or polygon edges). */
	circleRGBA(r, 16, 16, 12, WHITE, 128);
	aacircleRGBA(r, 48, 16, 12, WHITE, 128);
	filledCircleRGBA(r, 16, 48, 12, WHITE, 128);
	ellipseRGBA(r, 48, 48, 12, 6, WHITE, 128);
	filledEllipseRGBA(r, 48, 48, 6, 12, WHITE, 128);
	rectangleRGBA(r, 30, 30, 34, 34, WHITE, 128);
	thickLineRGBA(r, 2, 62, 62, 2, 5, 255, 0, 0, 128);
}

GOLDEN_TEST(pixel)
GOLDEN_TEST(hline_vline)
GOLDEN_TEST(rectangle)
GOLDEN_TEST(rounded_rectangle)
GOLDEN_TEST(box)
GOLDEN_TEST(rounded_box)
GOLDEN_TEST(line)
GOLDEN_TEST(aaline)
GOLDEN_TEST(thick_line)
GOLDEN_TEST(circle)
GOLDEN_TEST(aacircle)
GOLDEN_TEST(filled_circle)
GOLDEN_TEST(arc)
GOLDEN_TEST(ellipse)
GOLDEN_TEST(aaellipse)
GOLDEN_TEST(filled_ellipse)
GOLDEN_TEST(pie)
GOLDEN_TEST(filled_pie)
GOLDEN_TEST(trigon)
GOLDEN_TEST(polygon)
GOLDEN_TEST(aapolygon)
GOLDEN_TEST(filled_polygon)
GOLDEN_TEST(textured_polygon)
GOLDEN_TEST(bezier)
GOLDEN_TEST(text)
GOLDEN_TEST(blend_overlap)

/* --- API tests ------------------------------------------------------------------------------ */

/*
 * Each ...Color function must draw exactly like its ...RGBA counterpart with the color decoded as
 * 0xRRGGBBAA (API-001), or in memory byte order when the library is built with
 * SDL3_GFX_LEGACY_COLOR_BYTE_ORDER.
 */
static int compare_color_variants(int documented)
{
	GfxTestCanvas a, b;
	const Uint32 color = 0x3C78B4C8;
	Uint8 cr, cg, cb, ca;
	int i, ok = 1;

	if (documented) {
		cr = (Uint8)(color >> 24);
		cg = (Uint8)(color >> 16);
		cb = (Uint8)(color >> 8);
		ca = (Uint8)color;
	} else {
		const Uint8 *bytes = (const Uint8 *)&color;
		cr = bytes[0];
		cg = bytes[1];
		cb = bytes[2];
		ca = bytes[3];
	}
	if (GfxTest_CreateCanvas(&a, 64, 64) || GfxTest_CreateCanvas(&b, 64, 64)) {
		return TEST_ABORTED;
	}
	for (i = 0; i < 25; i++) {
		SDL_Renderer *ra = a.renderer, *rb = b.renderer;
		GfxTest_ClearCanvas(&a, 20, 20, 20);
		GfxTest_ClearCanvas(&b, 20, 20, 20);
		switch (i) {
			case 0:
				pixelColor(ra, 5, 5, color);
				pixelRGBA(rb, 5, 5, cr, cg, cb, ca);
				break;
			case 1:
				hlineColor(ra, 2, 50, 5, color);
				hlineRGBA(rb, 2, 50, 5, cr, cg, cb, ca);
				break;
			case 2:
				vlineColor(ra, 5, 2, 50, color);
				vlineRGBA(rb, 5, 2, 50, cr, cg, cb, ca);
				break;
			case 3:
				rectangleColor(ra, 2, 2, 50, 40, color);
				rectangleRGBA(rb, 2, 2, 50, 40, cr, cg, cb, ca);
				break;
			case 4:
				roundedRectangleColor(ra, 2, 2, 50, 40, 6, color);
				roundedRectangleRGBA(rb, 2, 2, 50, 40, 6, cr, cg, cb, ca);
				break;
			case 5:
				boxColor(ra, 2, 2, 50, 40, color);
				boxRGBA(rb, 2, 2, 50, 40, cr, cg, cb, ca);
				break;
			case 6:
				roundedBoxColor(ra, 2, 2, 50, 40, 6, color);
				roundedBoxRGBA(rb, 2, 2, 50, 40, 6, cr, cg, cb, ca);
				break;
			case 7:
				lineColor(ra, 2, 3, 60, 50, color);
				lineRGBA(rb, 2, 3, 60, 50, cr, cg, cb, ca);
				break;
			case 8:
				aalineColor(ra, 2, 3, 60, 50, color);
				aalineRGBA(rb, 2, 3, 60, 50, cr, cg, cb, ca);
				break;
			case 9:
				thickLineColor(ra, 2, 3, 60, 50, 4, color);
				thickLineRGBA(rb, 2, 3, 60, 50, 4, cr, cg, cb, ca);
				break;
			case 10:
				circleColor(ra, 32, 32, 20, color);
				circleRGBA(rb, 32, 32, 20, cr, cg, cb, ca);
				break;
			case 11:
				arcColor(ra, 32, 32, 20, 10, 200, color);
				arcRGBA(rb, 32, 32, 20, 10, 200, cr, cg, cb, ca);
				break;
			case 12:
				aacircleColor(ra, 32, 32, 20, color);
				aacircleRGBA(rb, 32, 32, 20, cr, cg, cb, ca);
				break;
			case 13:
				filledCircleColor(ra, 32, 32, 20, color);
				filledCircleRGBA(rb, 32, 32, 20, cr, cg, cb, ca);
				break;
			case 14:
				ellipseColor(ra, 32, 32, 20, 10, color);
				ellipseRGBA(rb, 32, 32, 20, 10, cr, cg, cb, ca);
				break;
			case 15:
				aaellipseColor(ra, 32, 32, 20, 10, color);
				aaellipseRGBA(rb, 32, 32, 20, 10, cr, cg, cb, ca);
				break;
			case 16:
				filledEllipseColor(ra, 32, 32, 20, 10, color);
				filledEllipseRGBA(rb, 32, 32, 20, 10, cr, cg, cb, ca);
				break;
			case 17:
				pieColor(ra, 32, 32, 20, 10, 200, color);
				pieRGBA(rb, 32, 32, 20, 10, 200, cr, cg, cb, ca);
				break;
			case 18:
				filledPieColor(ra, 32, 32, 20, 10, 200, color);
				filledPieRGBA(rb, 32, 32, 20, 10, 200, cr, cg, cb, ca);
				break;
			case 19:
				trigonColor(ra, 2, 2, 60, 10, 30, 60, color);
				trigonRGBA(rb, 2, 2, 60, 10, 30, 60, cr, cg, cb, ca);
				break;
			case 20:
				aatrigonColor(ra, 2, 2, 60, 10, 30, 60, color);
				aatrigonRGBA(rb, 2, 2, 60, 10, 30, 60, cr, cg, cb, ca);
				break;
			case 21:
				filledTrigonColor(ra, 2, 2, 60, 10, 30, 60, color);
				filledTrigonRGBA(rb, 2, 2, 60, 10, 30, 60, cr, cg, cb, ca);
				break;
			case 22:
				polygonColor(ra, star_x, star_y, 10, color);
				polygonRGBA(rb, star_x, star_y, 10, cr, cg, cb, ca);
				aapolygonColor(ra, concave_x, concave_y, 5, color);
				aapolygonRGBA(rb, concave_x, concave_y, 5, cr, cg, cb, ca);
				break;
			case 23:
				filledPolygonColor(ra, star_x, star_y, 10, color);
				filledPolygonRGBA(rb, star_x, star_y, 10, cr, cg, cb, ca);
				break;
			case 24:
				stringColor(ra, 2, 2, "Color", color);
				characterColor(ra, 2, 20, 'X', color);
				stringRGBA(rb, 2, 2, "Color", cr, cg, cb, ca);
				characterRGBA(rb, 2, 20, 'X', cr, cg, cb, ca);
				break;
			default:
				break;
		}
		SDL_FlushRenderer(ra);
		SDL_FlushRenderer(rb);
		if (SDLTest_CompareSurfaces(a.surface, b.surface, 0) != 0) {
			SDLTest_AssertCheck(0, "Color and RGBA variants differ for case %d", i);
			ok = 0;
		}
	}
	SDLTest_AssertCheck(ok, "All ...Color functions match their ...RGBA counterparts (%s byte order)",
		documented ? "documented 0xRRGGBBAA" : "memory");
	GfxTest_DestroyCanvas(&a);
	GfxTest_DestroyCanvas(&b);
	return TEST_COMPLETED;
}

static int api_color_equals_rgba(void *arg)
{
	(void)arg;
#ifdef SDL3_GFX_LEGACY_COLOR_BYTE_ORDER
	return compare_color_variants(0);
#else
	return compare_color_variants(1);
#endif
}

/* Documented error returns: -1 for invalid parameters. */
static int api_invalid_parameters(void *arg)
{
	GfxTestCanvas c;
	static const Sint16 vx[] = {1, 10, 5};
	static const Sint16 vy[] = {1, 1, 10};
	SDL_Surface *texture;
	(void)arg;

	if (GfxTest_CreateCanvas(&c, 32, 32)) {
		return TEST_ABORTED;
	}

	SDLTest_AssertCheck(polygonRGBA(c.renderer, vx, vy, 2, WHITE, 255) == -1, "polygonRGBA n=2 returns -1");
	SDLTest_AssertCheck(polygonRGBA(c.renderer, NULL, vy, 3, WHITE, 255) == -1, "polygonRGBA vx=NULL returns -1");
	SDLTest_AssertCheck(polygonRGBA(c.renderer, vx, NULL, 3, WHITE, 255) == -1, "polygonRGBA vy=NULL returns -1");
	SDLTest_AssertCheck(aapolygonRGBA(c.renderer, vx, vy, 2, WHITE, 255) == -1, "aapolygonRGBA n=2 returns -1");
	SDLTest_AssertCheck(aapolygonRGBA(c.renderer, NULL, vy, 3, WHITE, 255) == -1, "aapolygonRGBA vx=NULL returns -1");
	SDLTest_AssertCheck(filledPolygonRGBA(c.renderer, vx, vy, 2, WHITE, 255) == -1, "filledPolygonRGBA n=2 returns -1");
	SDLTest_AssertCheck(filledPolygonRGBA(c.renderer, NULL, vy, 3, WHITE, 255) == -1, "filledPolygonRGBA vx=NULL returns -1");
	SDLTest_AssertCheck(bezierRGBA(c.renderer, vx, vy, 2, 10, WHITE, 255) == -1, "bezierRGBA n=2 returns -1");
	SDLTest_AssertCheck(bezierRGBA(c.renderer, vx, vy, 3, 1, WHITE, 255) == -1, "bezierRGBA s=1 returns -1");
	SDLTest_AssertCheck(circleRGBA(c.renderer, 10, 10, -1, WHITE, 255) == -1, "circleRGBA rad<0 returns -1");
	SDLTest_AssertCheck(aacircleRGBA(c.renderer, 10, 10, -1, WHITE, 255) == -1, "aacircleRGBA rad<0 returns -1");
	SDLTest_AssertCheck(filledCircleRGBA(c.renderer, 10, 10, -1, WHITE, 255) == -1, "filledCircleRGBA rad<0 returns -1");
	SDLTest_AssertCheck(ellipseRGBA(c.renderer, 10, 10, -1, 5, WHITE, 255) == -1, "ellipseRGBA rx<0 returns -1");
	SDLTest_AssertCheck(aaellipseRGBA(c.renderer, 10, 10, 5, -1, WHITE, 255) == -1, "aaellipseRGBA ry<0 returns -1");
	SDLTest_AssertCheck(filledEllipseRGBA(c.renderer, 10, 10, -1, -1, WHITE, 255) == -1, "filledEllipseRGBA rx,ry<0 returns -1");
	SDLTest_AssertCheck(arcRGBA(c.renderer, 10, 10, -1, 0, 90, WHITE, 255) == -1, "arcRGBA rad<0 returns -1");
	SDLTest_AssertCheck(pieRGBA(c.renderer, 10, 10, -1, 0, 90, WHITE, 255) == -1, "pieRGBA rad<0 returns -1");
	SDLTest_AssertCheck(roundedBoxRGBA(c.renderer, 1, 1, 20, 20, -1, WHITE, 255) == -1, "roundedBoxRGBA rad<0 returns -1");
	SDLTest_AssertCheck(roundedRectangleRGBA(c.renderer, 1, 1, 20, 20, -1, WHITE, 255) == -1, "roundedRectangleRGBA rad<0 returns -1");
	SDLTest_AssertCheck(thickLineRGBA(c.renderer, 1, 1, 20, 20, 0, WHITE, 255) == -1, "thickLineRGBA width=0 returns -1");
	SDLTest_AssertCheck(texturedPolygon(c.renderer, vx, vy, 3, NULL, 0, 0) == -1, "texturedPolygon texture=NULL returns -1");
	texture = SDL_CreateSurface(4, 4, SDL_PIXELFORMAT_ARGB8888);
	SDLTest_AssertCheck(texturedPolygon(c.renderer, vx, vy, 2, texture, 0, 0) == -1, "texturedPolygon n=2 returns -1");
	SDL_DestroySurface(texture);

	/* Nothing must have been drawn by the failing calls. */
	SDLTest_AssertCheck(GfxTest_CountPixelsNot(&c, 0xFF000000) == 0, "Invalid calls did not draw");

	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}

/* Rounded boxes must stay inside their rectangle and be completely filled (PRIM-004). */
static int api_rounded_box_fill(void *arg)
{
	GfxTestCanvas c;
	static const Sint16 boxes[][5] = {{10, 10, 30, 50, 10}, {10, 10, 30, 50, 50}, {5, 20, 58, 30, 100}, {10, 10, 40, 40, 15}, {10, 10, 50, 30, 3}};
	int i, x, y, outside, holes;
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 64, 64)) {
		return TEST_ABORTED;
	}
	for (i = 0; i < (int)SDL_arraysize(boxes); i++) {
		const Sint16 x1 = boxes[i][0], y1 = boxes[i][1], x2 = boxes[i][2], y2 = boxes[i][3], rad = boxes[i][4];
		const int cx = (x1 + x2) / 2;
		GfxTest_ClearCanvas(&c, 0, 0, 0);
		roundedBoxRGBA(c.renderer, x1, y1, x2, y2, rad, WHITE, 255);
		outside = holes = 0;
		for (y = 0; y < 64; y++) {
			for (x = 0; x < 64; x++) {
				const int set = GfxTest_GetPixel(&c, x, y) != 0xFF000000;
				if (set && (x < x1 || x > x2 || y < y1 || y > y2)) {
					outside++;
				}
			}
			/* the center column is always inside a rounded box */
			if (y >= y1 && y <= y2 && GfxTest_GetPixel(&c, cx, y) == 0xFF000000) {
				holes++;
			}
		}
		SDLTest_AssertCheck(outside == 0 && holes == 0, "roundedBox(%d,%d,%d,%d,rad=%d): %d pixels outside, %d unfilled rows", x1, y1, x2,
			y2, rad, outside, holes);
	}
	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}

/* An arc from 0 to 360 degrees is a full circle (PRIM-005). */
static int api_arc_full_circle(void *arg)
{
	GfxTestCanvas a, b;
	(void)arg;
	if (GfxTest_CreateCanvas(&a, 32, 32) || GfxTest_CreateCanvas(&b, 32, 32)) {
		return TEST_ABORTED;
	}
	arcRGBA(a.renderer, 16, 16, 10, 0, 360, WHITE, 255);
	circleRGBA(b.renderer, 16, 16, 10, WHITE, 255);
	SDL_FlushRenderer(a.renderer);
	SDL_FlushRenderer(b.renderer);
	SDLTest_AssertCheck(GfxTest_CountPixelsNot(&a, 0xFF000000) > 0, "arc(0, 360) draws something");
	SDLTest_AssertCheck(SDLTest_CompareSurfaces(a.surface, b.surface, 0) == 0, "arc(0, 360) equals circle");
	GfxTest_DestroyCanvas(&a);
	GfxTest_DestroyCanvas(&b);
	return TEST_COMPLETED;
}
static SDL_Surface *create_solid_texture(void)
{
	SDL_Surface *s = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_ARGB8888);
	if (s != NULL) {
		SDL_FillSurfaceRect(s, NULL, 0xFFFFFFFF);
	}
	return s;
}

/* Polygons spanning most of the Sint16 range must not overflow the edge intersection math (PRIM-007). */
static int api_polygon_large_coords(void *arg)
{
	GfxTestCanvas c;
	SDL_Surface *texture;
	static const Sint16 vx[] = {-32768, 32767, 0};
	static const Sint16 vy[] = {-32768, -32768, 32767};
	(void)arg;
	texture = create_solid_texture();
	if (texture == NULL || GfxTest_CreateCanvas(&c, 64, 64)) {
		SDL_DestroySurface(texture);
		return TEST_ABORTED;
	}
	SDLTest_AssertCheck(filledPolygonRGBA(c.renderer, vx, vy, 3, WHITE, 255) == 0, "filledPolygonRGBA returns 0");
	SDLTest_AssertCheck(GfxTest_CountPixelsNot(&c, 0xFF000000) == 64 * 64, "filled polygon covers the canvas (%d pixels)",
		GfxTest_CountPixelsNot(&c, 0xFF000000));
	GfxTest_ClearCanvas(&c, 0, 0, 0);
	SDLTest_AssertCheck(texturedPolygon(c.renderer, vx, vy, 3, texture, 0, 0) == 0, "texturedPolygon returns 0 (PRIM-013)");
	SDLTest_AssertCheck(GfxTest_CountPixelsNot(&c, 0xFF000000) == 64 * 64, "textured polygon covers the canvas (%d pixels)",
		GfxTest_CountPixelsNot(&c, 0xFF000000));
	GfxTest_DestroyCanvas(&c);
	SDL_DestroySurface(texture);
	return TEST_COMPLETED;
}

static void make_star(Sint16 *vx, Sint16 *vy, int n, int cx, int cy, int r1, int r2)
{
	int i;
	for (i = 0; i < n; i++) {
		const double a = 2.0 * M_PI * i / n;
		const int r = (i & 1) ? r2 : r1;
		vx[i] = (Sint16)(cx + (int)SDL_floor(r * SDL_cos(a) + 0.5));
		vy[i] = (Sint16)(cy + (int)SDL_floor(r * SDL_sin(a) + 0.5));
	}
}

/* Polygon fills restricted to rows inside the viewport/clip rect must not change the output (PERF-001). */
static int api_polygon_viewport_clip(void *arg)
{
	GfxTestCanvas a, b;
	SDL_Surface *texture;
	Sint16 vx[24], vy[24], tx[24], ty[24];
	SDL_Rect viewport = {10, 20, 40, 30}, clip = {10, 20, 40, 30}, inner = {5, 3, 20, 12};
	int i, pass;
	(void)arg;
	texture = create_solid_texture();
	if (texture == NULL || GfxTest_CreateCanvas(&a, 64, 64) || GfxTest_CreateCanvas(&b, 64, 64)) {
		SDL_DestroySurface(texture);
		return TEST_ABORTED;
	}
	make_star(vx, vy, 24, 18, 12, 40, 15);
	for (i = 0; i < 24; i++) {
		tx[i] = vx[i] + 10;
		ty[i] = vy[i] + 20;
	}
	for (pass = 0; pass < 2; pass++) {
		/* a: viewport (plus a clip rect relative to it); b: translated with an equivalent clip rect */
		GfxTest_ClearCanvas(&a, 0, 0, 0);
		GfxTest_ClearCanvas(&b, 0, 0, 0);
		SDL_SetRenderViewport(a.renderer, &viewport);
		SDL_SetRenderClipRect(a.renderer, pass ? &inner : NULL);
		if (pass) {
			SDL_Rect translated = {inner.x + 10, inner.y + 20, inner.w, inner.h};
			SDL_SetRenderClipRect(b.renderer, &translated);
		} else {
			SDL_SetRenderClipRect(b.renderer, &clip);
		}
		filledPolygonRGBA(a.renderer, vx, vy, 24, 255, 0, 0, 255);
		filledPolygonRGBA(b.renderer, tx, ty, 24, 255, 0, 0, 255);
		texturedPolygon(a.renderer, vx, vy, 5, texture, 0, 0);
		texturedPolygon(b.renderer, tx, ty, 5, texture, 10, -20);
		SDL_FlushRenderer(a.renderer);
		SDL_FlushRenderer(b.renderer);
		SDLTest_AssertCheck(GfxTest_CountPixelsNot(&a, 0xFF000000) > 0, "pass %d: polygon visible", pass);
		SDLTest_AssertCheck(SDLTest_CompareSurfaces(a.surface, b.surface, 0) == 0, "pass %d: viewport drawing equals translated drawing", pass);
		SDL_SetRenderViewport(a.renderer, NULL);
		SDL_SetRenderClipRect(a.renderer, NULL);
		SDL_SetRenderClipRect(b.renderer, NULL);
	}

	/* With a render scale the visible rows are in logical coordinates. SDL3's software renderer drops
	   rectangles and lines thinner than one pixel after scaling (PRIM-017), so the rows of a scale 0.5
	   fill are not visible; check the logical size of the visible area instead. */
	vx[0] = 0, vy[0] = 0, vx[1] = 127, vy[1] = 0, vx[2] = 127, vy[2] = 127, vx[3] = 0, vy[3] = 127;
	GfxTest_ClearCanvas(&a, 0, 0, 0);
	SDL_SetRenderScale(a.renderer, 0.5f, 0.5f);
	{
		SDL_Rect logical;
		SDLTest_AssertCheck(SDL_GetRenderViewport(a.renderer, &logical) && logical.h == 128, "scale 0.5: the visible area has 128 logical rows");
	}
	SDLTest_AssertCheck(filledPolygonRGBA(a.renderer, vx, vy, 4, WHITE, 255) == 0, "scale 0.5: fill succeeds");
	SDL_SetRenderScale(a.renderer, 1.0f, 1.0f);

	GfxTest_DestroyCanvas(&a);
	GfxTest_DestroyCanvas(&b);
	SDL_DestroySurface(texture);
	return TEST_COMPLETED;
}

#define POLY_THREADS    4
#define POLY_ITERATIONS 300

static Uint32 polygon_workload(void)
{
	GfxTestCanvas c;
	Sint16 vx[200], vy[200];
	Uint32 sum = 0;
	int i, n;
	if (GfxTest_CreateCanvas(&c, 64, 64)) {
		return 0;
	}
	for (i = 0; i < POLY_ITERATIONS; i++) {
		n = 4 + (i * 37) % 196;
		make_star(vx, vy, n, 32, 32, 30, 10 + i % 15);
		GfxTest_ClearCanvas(&c, 0, 0, 0);
		filledPolygonRGBA(c.renderer, vx, vy, n, WHITE, 255);
		sum = sum * 31u + (Uint32)GfxTest_CountPixelsNot(&c, 0xFF000000);
	}
	GfxTest_DestroyCanvas(&c);
	return sum;
}

static int SDLCALL polygon_thread(void *data)
{
	*(Uint32 *)data = polygon_workload();
	return 0;
}

/* filledPolygon without MT buffers must be thread-safe (PRIM-002). */
static int api_polygon_threads(void *arg)
{
	SDL_Thread *threads[POLY_THREADS];
	Uint32 sums[POLY_THREADS];
	Uint32 expected;
	int i;
	(void)arg;
	expected = polygon_workload();
	for (i = 0; i < POLY_THREADS; i++) {
		sums[i] = 0;
		threads[i] = SDL_CreateThread(polygon_thread, "poly", &sums[i]);
	}
	for (i = 0; i < POLY_THREADS; i++) {
		SDL_WaitThread(threads[i], NULL);
		SDLTest_AssertCheck(sums[i] == expected, "thread %d result %08x equals single-threaded %08x", i, sums[i], expected);
	}
	return TEST_COMPLETED;
}
/* Blended one-pixel lines must be drawn; SDL3's software renderer drops zero-length blended lines (PRIM-015). */
static int api_degenerate_lines_blended(void *arg)
{
	GfxTestCanvas c;
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 16, 16)) {
		return TEST_ABORTED;
	}
	hlineRGBA(c.renderer, 2, 2, 2, 255, 255, 255, 128);
	vlineRGBA(c.renderer, 5, 5, 5, 255, 255, 255, 128);
	lineRGBA(c.renderer, 8, 8, 8, 8, 255, 255, 255, 128);
	hlineRGBA(c.renderer, 10, 12, 10, 255, 255, 255, 128);
	SDLTest_AssertCheck(GfxTest_GetPixel(&c, 2, 2) != 0xFF000000, "hlineRGBA(x1 == x2) blended draws a pixel");
	SDLTest_AssertCheck(GfxTest_GetPixel(&c, 5, 5) != 0xFF000000, "vlineRGBA(y1 == y2) blended draws a pixel");
	SDLTest_AssertCheck(GfxTest_GetPixel(&c, 8, 8) != 0xFF000000, "lineRGBA(single point) blended draws a pixel");
	SDLTest_AssertCheck(GfxTest_CountPixelsNot(&c, 0xFF000000) == 6, "exactly 6 pixels drawn (%d)", GfxTest_CountPixelsNot(&c, 0xFF000000));
	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}
/* Anti-aliased lines spanning 32768 pixels or more follow their slope (PRIM-018: the 16.16 error step was computed
   with signed shifts that overflowed). The line passes through the origin with slope 2/3. */
static int api_aaline_long(void *arg)
{
	GfxTestCanvas c;
	int x, y, errors = 0;
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 64, 64)) {
		return TEST_ABORTED;
	}
	aalineRGBA(c.renderer, -30000, -20000, 30000, 20000, WHITE, 255);
	for (x = 3; x < 60; x += 3) {
		int best = -1, bestv = 0;
		for (y = 0; y < 64; y++) {
			int v = (int)(GfxTest_GetPixel(&c, x, y) & 0xFF);
			if (v > bestv) {
				bestv = v;
				best = y;
			}
		}
		errors += (best < 0) || (SDL_abs(best - x * 2 / 3) > 1);
	}
	SDLTest_AssertCheck(errors == 0, "Long anti-aliased line has slope 2/3 (%d columns off the line)", errors);
	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}

/* Largest deviation (in pixels) of the drawn edge of an ellipse with center (cx, cy) and radii (rx, ry) from the exact
   curve. horizontal: the right edge (brightest pixel of each row), else the bottom edge (brightest pixel of each column). */
static int ellipse_edge_error(GfxTestCanvas *c, int cx, int cy, int rx, int ry, int horizontal)
{
	int i, j, worst = 0;
	for (i = 0; i < 64; i++) {
		int best = -1;
		Uint32 bestv = 0;
		double d, expected;
		for (j = 0; j < 64; j++) {
			Uint32 v = GfxTest_GetPixel(c, horizontal ? j : i, horizontal ? i : j) & 0xFF;
			if (v >= bestv && v > 0) {
				bestv = v;
				best = j;
			}
		}
		d = horizontal ? (double)(i - cy) / ry : (double)(i - cx) / rx;
		expected = horizontal ? cx + rx * SDL_sqrt(1.0 - d * d) : cy + ry * SDL_sqrt(1.0 - d * d);
		worst = SDL_max(worst, best < 0 ? 64 : (int)SDL_fabs(best - expected));
	}
	return worst;
}

/* Ellipses with large radii keep their shape (PRIM-019: the 32-bit error terms of the midpoint algorithms overflowed). */
static int api_ellipse_large(void *arg)
{
	GfxTestCanvas c;
	int k, err;
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 64, 64)) {
		return TEST_ABORTED;
	}
	for (k = 0; k < 6; k++) {
		const int horizontal = k & 1;
		/* The visible edge passes through the canvas center: right end (horizontal) or bottom end of the ellipse */
		const int rx = 3000, ry = 1800, cx = horizontal ? 32 - rx : 32, cy = horizontal ? 32 : 32 - ry;
		GfxTest_ClearCanvas(&c, 0, 0, 0);
		if (k < 2) {
			ellipseRGBA(c.renderer, (Sint16)cx, (Sint16)cy, (Sint16)rx, (Sint16)ry, WHITE, 255);
		} else if (k < 4) {
			aaellipseRGBA(c.renderer, (Sint16)cx, (Sint16)cy, (Sint16)rx, (Sint16)ry, WHITE, 255);
		} else {
			filledEllipseRGBA(c.renderer, (Sint16)cx, (Sint16)cy, (Sint16)rx, (Sint16)ry, WHITE, 255);
		}
		err = ellipse_edge_error(&c, cx, cy, rx, ry, horizontal);
		SDLTest_AssertCheck(err <= 1, "%s, %s edge: largest deviation %d pixels", k < 2 ? "ellipse" : (k < 4 ? "aaellipse" : "filledEllipse"),
			horizontal ? "right" : "bottom", err);
	}
	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}

/* A zero-length thick line of width w is a w x w square centered on the point (PRIM-010). */
static int api_thick_point(void *arg)
{
	GfxTestCanvas c;
	int x, y, x1 = 64, y1 = 64, x2 = -1, y2 = -1;
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 64, 64)) {
		return TEST_ABORTED;
	}
	thickLineRGBA(c.renderer, 20, 20, 20, 20, 6, WHITE, 255);
	for (y = 0; y < 64; y++) {
		for (x = 0; x < 64; x++) {
			if (GfxTest_GetPixel(&c, x, y) != 0xFF000000) {
				x1 = SDL_min(x1, x);
				y1 = SDL_min(y1, y);
				x2 = SDL_max(x2, x);
				y2 = SDL_max(y2, y);
			}
		}
	}
	SDLTest_AssertCheck(x2 - x1 + 1 == 6 && y2 - y1 + 1 == 6, "Thick point of width 6 is 6x6 (drawn: %dx%d at %d,%d)", x2 - x1 + 1, y2 - y1 + 1, x1, y1);
	SDLTest_AssertCheck(x1 == 17 && y1 == 17, "Thick point is centered on (20,20) (top left %d,%d)", x1, y1);
	GfxTest_DestroyCanvas(&c);
	return TEST_COMPLETED;
}
/* Every pixel of a semi-transparent primitive is blended exactly once (no darker seams, corners or joins). */
static int api_blend_once(void *arg)
{
	GfxTestCanvas c, ref;
	Uint32 expected;
	int i, x, y, drawn, wrong;
	static const Sint16 px[] = {5, 58, 50, 30, 8};
	static const Sint16 py[] = {4, 10, 60, 30, 50};
	static const char *names[] = {"pixel", "hline", "vline", "rectangle", "roundedRectangle", "box", "roundedBox", "line", "circle", "arc",
		"ellipse", "filledCircle", "filledEllipse", "pie", "filledPie", "trigon", "filledTrigon", "polygon", "filledPolygon", "bezier",
		"thickLine"};
	(void)arg;
	if (GfxTest_CreateCanvas(&c, 64, 64) || GfxTest_CreateCanvas(&ref, 1, 1)) {
		return TEST_ABORTED;
	}
	pixelRGBA(ref.renderer, 0, 0, 255, 255, 255, 128);
	expected = GfxTest_GetPixel(&ref, 0, 0);
	for (i = 0; i < (int)SDL_arraysize(names); i++) {
		GfxTest_ClearCanvas(&c, 0, 0, 0);
		switch (i) {
			case 0:
				pixelRGBA(c.renderer, 5, 5, WHITE, 128);
				break;
			case 1:
				hlineRGBA(c.renderer, 5, 50, 10, WHITE, 128);
				break;
			case 2:
				vlineRGBA(c.renderer, 10, 5, 50, WHITE, 128);
				break;
			case 3:
				rectangleRGBA(c.renderer, 5, 5, 50, 40, WHITE, 128);
				break;
			case 4:
				roundedRectangleRGBA(c.renderer, 5, 5, 50, 40, 8, WHITE, 128);
				break;
			case 5:
				boxRGBA(c.renderer, 5, 5, 50, 40, WHITE, 128);
				break;
			case 6:
				roundedBoxRGBA(c.renderer, 5, 5, 50, 40, 8, WHITE, 128);
				break;
			case 7:
				lineRGBA(c.renderer, 3, 7, 60, 50, WHITE, 128);
				break;
			case 8:
				circleRGBA(c.renderer, 32, 32, 20, WHITE, 128);
				break;
			case 9:
				arcRGBA(c.renderer, 32, 32, 20, 10, 300, WHITE, 128);
				break;
			case 10:
				ellipseRGBA(c.renderer, 32, 32, 28, 12, WHITE, 128);
				break;
			case 11:
				filledCircleRGBA(c.renderer, 32, 32, 20, WHITE, 128);
				break;
			case 12:
				filledEllipseRGBA(c.renderer, 32, 32, 28, 12, WHITE, 128);
				break;
			case 13:
				pieRGBA(c.renderer, 32, 32, 20, 30, 250, WHITE, 128);
				break;
			case 14:
				filledPieRGBA(c.renderer, 32, 32, 20, 30, 250, WHITE, 128);
				break;
			case 15:
				trigonRGBA(c.renderer, 5, 5, 60, 20, 20, 60, WHITE, 128);
				break;
			case 16:
				filledTrigonRGBA(c.renderer, 5, 5, 60, 20, 20, 60, WHITE, 128);
				break;
			case 17:
				polygonRGBA(c.renderer, px, py, 5, WHITE, 128);
				break;
			case 18:
				filledPolygonRGBA(c.renderer, px, py, 5, WHITE, 128);
				break;
			case 19:
				bezierRGBA(c.renderer, px, py, 5, 20, WHITE, 128);
				break;
			default:
				thickLineRGBA(c.renderer, 5, 10, 58, 50, 5, WHITE, 128);
				break;
		}
		drawn = wrong = 0;
		for (y = 0; y < 64; y++) {
			for (x = 0; x < 64; x++) {
				const Uint32 p = GfxTest_GetPixel(&c, x, y);
				if (p != 0xFF000000) {
					drawn++;
					if (p != expected && wrong++ < 4) {
						SDLTest_Log("%s: pixel (%d,%d) is 0x%08x", names[i], x, y, (unsigned int)p);
					}
				}
			}
		}
		SDLTest_AssertCheck(drawn > 0 && wrong == 0, "%s: every pixel blended once (%d of %d pixels differ)", names[i], wrong, drawn);
	}
	GfxTest_DestroyCanvas(&c);
	GfxTest_DestroyCanvas(&ref);
	return TEST_COMPLETED;
}
/* Oversized custom fonts are rejected like a NULL font (size math stays in range). */
static int api_font_too_large(void *arg)
{
	static unsigned char dummy[16];
	GfxTestCanvas a, b;
	(void)arg;
	if (GfxTest_CreateCanvas(&a, 16, 16) || GfxTest_CreateCanvas(&b, 16, 16)) {
		return TEST_ABORTED;
	}
	gfxPrimitivesSetFont(dummy, 8, 0x01000000);
	characterRGBA(a.renderer, 4, 4, 'A', WHITE, 255);
	gfxPrimitivesSetFont(NULL, 0, 0);
	characterRGBA(b.renderer, 4, 4, 'A', WHITE, 255);
	SDL_FlushRenderer(a.renderer);
	SDL_FlushRenderer(b.renderer);
	SDLTest_AssertCheck(SDLTest_CompareSurfaces(a.surface, b.surface, 0) == 0, "8 x 16M font falls back to the built-in font");
	gfxPrimitivesSetFont(NULL, 0, 0);
	GfxTest_DestroyCanvas(&a);
	GfxTest_DestroyCanvas(&b);
	return TEST_COMPLETED;
}
/* Drawing with a NULL renderer must fail cleanly. */
static int api_null_renderer(void *arg)
{
	static const Sint16 vx[] = {1, 10, 5};
	static const Sint16 vy[] = {1, 1, 10};
	(void)arg;
	SDLTest_AssertCheck(pixelRGBA(NULL, 1, 1, WHITE, 255) == -1, "pixelRGBA(NULL) returns -1");
	SDLTest_AssertCheck(hlineRGBA(NULL, 1, 5, 1, WHITE, 255) == -1, "hlineRGBA(NULL) returns -1");
	SDLTest_AssertCheck(lineRGBA(NULL, 1, 1, 5, 5, WHITE, 255) == -1, "lineRGBA(NULL) returns -1");
	SDLTest_AssertCheck(aalineRGBA(NULL, 1, 1, 5, 9, WHITE, 255) == -1, "aalineRGBA(NULL) returns -1");
	SDLTest_AssertCheck(boxRGBA(NULL, 1, 1, 5, 5, WHITE, 255) == -1, "boxRGBA(NULL) returns -1");
	SDLTest_AssertCheck(circleRGBA(NULL, 10, 10, 5, WHITE, 255) == -1, "circleRGBA(NULL) returns -1");
	SDLTest_AssertCheck(filledPolygonRGBA(NULL, vx, vy, 3, WHITE, 255) == -1, "filledPolygonRGBA(NULL) returns -1");
	return TEST_COMPLETED;
}

/* The font cache must keep working when the renderer changes (finding FONT-001). */
static int api_text_after_renderer_change(void *arg)
{
	GfxTestCanvas c;
	int i;
	(void)arg;
	for (i = 0; i < 2; i++) {
		if (GfxTest_CreateCanvas(&c, 32, 16)) {
			return TEST_ABORTED;
		}
		SDLTest_AssertCheck(stringRGBA(c.renderer, 0, 0, "Ab", WHITE, 255) == 0, "stringRGBA succeeds on renderer %d", i);
		SDLTest_AssertCheck(GfxTest_CountPixelsNot(&c, 0xFF000000) > 0, "Text drawn on renderer %d", i);
		GfxTest_DestroyCanvas(&c); /* releases the glyph cache before the renderer (documented usage) */
	}
	return TEST_COMPLETED;
}

/* Two live renderers drawing text alternately must both render correctly (FONT-001). */
static int api_text_two_renderers(void *arg)
{
	GfxTestCanvas a, b;
	int round;
	(void)arg;
	if (GfxTest_CreateCanvas(&a, 40, 16) || GfxTest_CreateCanvas(&b, 40, 16)) {
		return TEST_ABORTED;
	}
	for (round = 0; round < 2; round++) {
		GfxTest_ClearCanvas(&a, 0, 0, 0);
		GfxTest_ClearCanvas(&b, 0, 0, 0);
		SDLTest_AssertCheck(stringRGBA(a.renderer, 1, 1, "Ab1", WHITE, 255) == 0, "Text on renderer A (round %d)", round);
		SDLTest_AssertCheck(stringRGBA(b.renderer, 1, 1, "Ab1", WHITE, 255) == 0, "Text on renderer B (round %d)", round);
		SDL_FlushRenderer(a.renderer);
		SDL_FlushRenderer(b.renderer);
		SDLTest_AssertCheck(GfxTest_CountPixelsNot(&b, 0xFF000000) > 0, "Renderer B shows text (round %d)", round);
		SDLTest_AssertCheck(SDLTest_CompareSurfaces(a.surface, b.surface, 0) == 0, "Both renderers show the same text (round %d)", round);
	}
	GfxTest_DestroyCanvas(&b);
	GfxTest_DestroyCanvas(&a);
	return TEST_COMPLETED;
}
static const SDLTest_TestCaseReference api_color_ref = {api_color_equals_rgba, "api_color_equals_rgba", "...Color == ...RGBA (0xRRGGBBAA, API-001)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_invalid_ref = {api_invalid_parameters, "api_invalid_parameters", "Invalid parameters return -1", TEST_ENABLED};
static const SDLTest_TestCaseReference api_rbox_ref = {api_rounded_box_fill, "api_rounded_box_fill", "Rounded box bounds and fill (PRIM-004)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_arc_ref = {api_arc_full_circle, "api_arc_full_circle", "arc 0..360 (PRIM-005)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_poly_large_ref = {api_polygon_large_coords, "api_polygon_large_coords", "Polygon fill with large coordinates (PRIM-007, PRIM-013)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_poly_clip_ref = {api_polygon_viewport_clip, "api_polygon_viewport_clip", "Polygon fill row clipping (PERF-001)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_poly_threads_ref = {api_polygon_threads, "api_polygon_threads", "Polygon fill from several threads (PRIM-002)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_degenerate_ref = {api_degenerate_lines_blended, "api_degenerate_lines_blended", "Blended one-pixel lines (PRIM-015)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_aaline_long_ref = {api_aaline_long, "api_aaline_long", "Anti-aliased line longer than 32767 pixels (PRIM-018)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_ellipse_large_ref = {api_ellipse_large, "api_ellipse_large", "Ellipses with large radii (PRIM-019)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_thick_point_ref = {api_thick_point, "api_thick_point", "Zero-length thick line is a centered square (PRIM-010)", TEST_ENABLED};
static const SDLTest_TestCaseReference api_blend_once_ref = {api_blend_once, "api_blend_once", "Semi-transparent primitives blend each pixel once", TEST_ENABLED};
static const SDLTest_TestCaseReference api_font_large_ref = {api_font_too_large, "api_font_too_large", "Oversized custom font", TEST_ENABLED};
static const SDLTest_TestCaseReference api_null_ref = {api_null_renderer, "api_null_renderer", "NULL renderer returns -1", TEST_ENABLED};
static const SDLTest_TestCaseReference api_text_ref = {api_text_after_renderer_change, "api_text_after_renderer_change", "Text with a new renderer", TEST_ENABLED};
static const SDLTest_TestCaseReference api_text2_ref = {api_text_two_renderers, "api_text_two_renderers", "Text on two renderers (FONT-001)", TEST_ENABLED};

static const SDLTest_TestCaseReference *primitivesTests[] = {
	&golden_pixel_ref, &golden_hline_vline_ref, &golden_rectangle_ref, &golden_rounded_rectangle_ref, &golden_box_ref,
	&golden_rounded_box_ref, &golden_line_ref, &golden_aaline_ref, &golden_thick_line_ref, &golden_circle_ref,
	&golden_aacircle_ref, &golden_filled_circle_ref, &golden_arc_ref, &golden_ellipse_ref, &golden_aaellipse_ref,
	&golden_filled_ellipse_ref, &golden_pie_ref, &golden_filled_pie_ref, &golden_trigon_ref, &golden_polygon_ref,
	&golden_aapolygon_ref, &golden_filled_polygon_ref, &golden_textured_polygon_ref, &golden_bezier_ref, &golden_text_ref,
	&golden_blend_overlap_ref, &api_color_ref, &api_invalid_ref, &api_rbox_ref, &api_arc_ref, &api_poly_large_ref, &api_poly_clip_ref, &api_poly_threads_ref, &api_degenerate_ref, &api_thick_point_ref, &api_aaline_long_ref, &api_ellipse_large_ref, &api_blend_once_ref, &api_font_large_ref, &api_null_ref, &api_text_ref, &api_text2_ref, NULL};

SDLTest_TestSuiteReference primitivesTestSuite = {"Primitives", NULL, primitivesTests, NULL};
