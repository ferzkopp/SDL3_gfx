/*
 * Fuzz target: custom fonts. gfxPrimitivesSetFont() is given a font of cw x ch pixels per glyph
 * (256 glyphs, rows padded to whole bytes, i.e. ((cw + 7) / 8) * ch bytes per glyph) built from the
 * input, then characters and strings are drawn with all four rotations.
 */
#include "fuzz_common.h"

#include <stdlib.h>

#include "SDL3_gfxPrimitives.h"

static SDL_Surface *surface;
static SDL_Renderer *renderer;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzReader in;
	Uint32 cw, ch, glyph_bytes, i, rot;
	unsigned char *font;
	char text[17];

	if (renderer == NULL) {
		surface = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_ARGB8888);
		renderer = SDL_CreateSoftwareRenderer(surface);
		if (renderer == NULL) {
			return 0;
		}
	}

	fuzz_init(&in, data, size);
	cw = 1 + fuzz_u8(&in) % 32;
	ch = 1 + fuzz_u8(&in) % 32;
	glyph_bytes = ((cw + 7) / 8) * ch;
	font = (unsigned char *)malloc(256 * glyph_bytes);
	if (font == NULL) {
		return 0;
	}
	for (i = 0; i < 256 * glyph_bytes; i++) {
		font[i] = fuzz_u8(&in);
	}
	for (i = 0; i < sizeof(text) - 1; i++) {
		text[i] = (char)fuzz_u8(&in);
		if (text[i] == 0) {
			break;
		}
	}
	text[i] = 0;

	gfxPrimitivesSetFont(font, cw, ch);
	for (rot = 0; rot < 5; rot++) {
		gfxPrimitivesSetFontRotation(rot);
		stringRGBA(renderer, (Sint16)fuzz_coord(&in), (Sint16)fuzz_coord(&in), text, 255, 255, 255, fuzz_u8(&in));
		characterColor(renderer, 10, 10, (char)fuzz_u8(&in), fuzz_u32(&in));
	}
	gfxPrimitivesSetFontRotation(0);
	/* Back to the built-in font before the custom font memory is released. */
	gfxPrimitivesSetFont(NULL, 0, 0);
	free(font);
	SDL_FlushRenderer(renderer);
	return 0;
}
