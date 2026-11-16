/*
 * Helpers shared by the SDL3_gfx fuzz targets: a byte reader that turns the fuzzer input into
 * parameters. Reads past the end return 0, so every input is valid.
 */
#ifndef SDL3_GFX_FUZZ_COMMON_H
#define SDL3_GFX_FUZZ_COMMON_H

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct FuzzReader {
	const uint8_t *data;
	size_t size;
	size_t pos;
} FuzzReader;

static void fuzz_init(FuzzReader *r, const uint8_t *data, size_t size)
{
	r->data = data;
	r->size = size;
	r->pos = 0;
}

static int fuzz_done(const FuzzReader *r)
{
	return r->pos >= r->size;
}

static Uint8 fuzz_u8(FuzzReader *r)
{
	return (Uint8)(r->pos < r->size ? r->data[r->pos++] : 0);
}

static Uint16 fuzz_u16(FuzzReader *r)
{
	Uint16 lo = fuzz_u8(r);
	return (Uint16)(lo | (fuzz_u8(r) << 8));
}

static Uint32 fuzz_u32(FuzzReader *r)
{
	Uint32 lo = fuzz_u16(r);
	return lo | ((Uint32)fuzz_u16(r) << 16);
}

/* Coordinates: mostly near the canvas, sometimes extreme values to exercise clipping/overflow. */
static Sint16 fuzz_coord(FuzzReader *r)
{
	Uint8 mode = fuzz_u8(r);
	switch (mode & 7) {
		case 0:
			return (Sint16)fuzz_u16(r);
		case 1:
			return (Sint16)((mode & 0x80) ? 32767 : -32768);
		default:
			return (Sint16)((Sint8)fuzz_u8(r)) + 32;
	}
}

static double fuzz_double(FuzzReader *r)
{
	Uint8 mode = fuzz_u8(r);
	switch (mode & 15) {
		case 0: {
			double d;
			Uint32 lo = fuzz_u32(r), hi = fuzz_u32(r);
			Uint64 bits = ((Uint64)hi << 32) | lo;
			memcpy(&d, &bits, sizeof(d)); /* any bit pattern incl. NaN/Inf/denormals */
			return d;
		}
		case 1:
			return 0.0;
		case 2:
			return -0.0;
		default:
			return ((double)(Sint16)fuzz_u16(r)) / 1024.0;
	}
}

#endif
