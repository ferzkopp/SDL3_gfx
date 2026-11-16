/*
 * Fuzz target: SDL3_gfxPrimitives. The input is decoded into a sequence of drawing commands with
 * arbitrary parameters (coordinates incl. extreme values, radii, angles, polygon vertex arrays,
 * Bezier steps, line widths, text) executed on a 64x64 software renderer.
 */
#include "fuzz_common.h"

#include "SDL3_gfxPrimitives.h"

#define MAX_COMMANDS 32
#define MAX_VERTICES 32

static SDL_Surface *surface;
static SDL_Renderer *renderer;
static SDL_Surface *texture;

static void setup(void)
{
	if (renderer) {
		return;
	}
	surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
	renderer = SDL_CreateSoftwareRenderer(surface);
	texture = SDL_CreateSurface(7, 5, SDL_PIXELFORMAT_ARGB8888);
	if (texture) {
		SDL_FillSurfaceRect(texture, NULL, 0xFF8040C0);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzReader in;
	int cmd;
	Sint16 vx[MAX_VERTICES], vy[MAX_VERTICES];
	char text[33];

	setup();
	if (renderer == NULL) {
		return 0;
	}
	fuzz_init(&in, data, size);

	for (cmd = 0; cmd < MAX_COMMANDS && !fuzz_done(&in); cmd++) {
		Uint8 op = fuzz_u8(&in);
		Sint16 x1 = fuzz_coord(&in), y1 = fuzz_coord(&in), x2 = fuzz_coord(&in), y2 = fuzz_coord(&in);
		Sint16 x3 = fuzz_coord(&in), y3 = fuzz_coord(&in);
		Sint16 rad = (Sint16)fuzz_u16(&in);
		Uint8 r = fuzz_u8(&in), g = fuzz_u8(&in), b = fuzz_u8(&in), a = fuzz_u8(&in);
		Uint32 color = fuzz_u32(&in);
		int n = fuzz_u8(&in) % (MAX_VERTICES + 1), i;

		for (i = 0; i < n; i++) {
			vx[i] = fuzz_coord(&in);
			vy[i] = fuzz_coord(&in);
		}

		switch (op % 32) {
			case 0:
				pixelRGBA(renderer, x1, y1, r, g, b, a);
				pixelColor(renderer, x2, y2, color);
				break;
			case 1:
				hlineRGBA(renderer, x1, x2, y1, r, g, b, a);
				vlineColor(renderer, x1, y1, y2, color);
				break;
			case 2:
				rectangleRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
				break;
			case 3:
				roundedRectangleRGBA(renderer, x1, y1, x2, y2, rad, r, g, b, a);
				break;
			case 4:
				boxRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
				break;
			case 5:
				roundedBoxRGBA(renderer, x1, y1, x2, y2, rad, r, g, b, a);
				break;
			case 6:
				lineRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
				break;
			case 7:
				aalineRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
				break;
			case 8:
				thickLineRGBA(renderer, x1, y1, x2, y2, (Uint8)rad, r, g, b, a);
				break;
			case 9:
				circleRGBA(renderer, x1, y1, rad, r, g, b, a);
				break;
			case 10:
				arcRGBA(renderer, x1, y1, rad, x2, y2, r, g, b, a);
				break;
			case 11:
				aacircleRGBA(renderer, x1, y1, rad, r, g, b, a);
				break;
			case 12:
				filledCircleRGBA(renderer, x1, y1, rad, r, g, b, a);
				break;
			case 13:
				ellipseRGBA(renderer, x1, y1, rad, x2, r, g, b, a);
				break;
			case 14:
				aaellipseRGBA(renderer, x1, y1, rad, x2, r, g, b, a);
				break;
			case 15:
				filledEllipseRGBA(renderer, x1, y1, rad, x2, r, g, b, a);
				break;
			case 16:
				pieRGBA(renderer, x1, y1, rad, x2, y2, r, g, b, a);
				break;
			case 17:
				filledPieRGBA(renderer, x1, y1, rad, x2, y2, r, g, b, a);
				break;
			case 18:
				trigonRGBA(renderer, x1, y1, x2, y2, x3, y3, r, g, b, a);
				break;
			case 19:
				aatrigonRGBA(renderer, x1, y1, x2, y2, x3, y3, r, g, b, a);
				break;
			case 20:
				filledTrigonRGBA(renderer, x1, y1, x2, y2, x3, y3, r, g, b, a);
				break;
			case 21:
				polygonRGBA(renderer, vx, vy, n, r, g, b, a);
				break;
			case 22:
				aapolygonRGBA(renderer, vx, vy, n, r, g, b, a);
				break;
			case 23:
				filledPolygonRGBA(renderer, vx, vy, n, r, g, b, a);
				break;
			case 24:
				texturedPolygon(renderer, vx, vy, n, texture, x1, y1);
				break;
			case 25:
				bezierRGBA(renderer, vx, vy, n, rad % 64, r, g, b, a);
				break;
			case 26:
				for (i = 0; i < (int)sizeof(text) - 1; i++) {
					text[i] = (char)fuzz_u8(&in);
					if (text[i] == 0) {
						break;
					}
				}
				text[i] = 0;
				gfxPrimitivesSetFontRotation((Uint32)(op >> 5));
				stringRGBA(renderer, x1, y1, text, r, g, b, a);
				gfxPrimitivesSetFontRotation(0);
				break;
			case 27:
				characterColor(renderer, x1, y1, (char)r, color);
				break;
			case 28:
				thickLineColor(renderer, x1, y1, x2, y2, (Uint8)(rad & 0x0F), color);
				break;
			case 29:
				filledPolygonColor(renderer, vx, vy, n, color);
				break;
			case 30:
				aaellipseColor(renderer, x1, y1, (Sint16)(rad & 0x3FF), (Sint16)(x2 & 0x3FF), color);
				break;
			default:
				arcColor(renderer, x1, y1, (Sint16)(rad & 0x3FF), x2, y2, color);
				break;
		}
	}
	SDL_FlushRenderer(renderer);
	return 0;
}
