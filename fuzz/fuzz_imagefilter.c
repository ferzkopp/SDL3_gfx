/*
 * Fuzz target: SDL3_imageFilter. Every function is called with exactly sized heap buffers (so
 * AddressSanitizer catches any access beyond 'length'), arbitrary parameters, and both with the
 * MMX-detecting default path and the C path.
 */
#include "fuzz_common.h"

#include <stdlib.h>

#include "SDL3_imageFilter.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	FuzzReader in;
	unsigned int len, i;
	unsigned char *s1, *s2, *d;
	Uint8 op, p1, p2;
	int ci[4];

	fuzz_init(&in, data, size);
	op = fuzz_u8(&in);
	len = fuzz_u16(&in) % 4097;
	p1 = fuzz_u8(&in);
	p2 = fuzz_u8(&in);
	for (i = 0; i < 4; i++) {
		ci[i] = (int)fuzz_u32(&in);
	}

	/* malloc(0) may return NULL; use at least one byte but pass the real length. */
	s1 = (unsigned char *)malloc(len ? len : 1);
	s2 = (unsigned char *)malloc(len ? len : 1);
	d = (unsigned char *)malloc(len ? len : 1);
	if (!s1 || !s2 || !d) {
		free(s1);
		free(s2);
		free(d);
		return 0;
	}
	for (i = 0; i < len; i++) {
		s1[i] = fuzz_u8(&in);
		s2[i] = fuzz_u8(&in);
	}

	if (op & 0x80) {
		SDL_imageFilterMMXoff();
	} else {
		SDL_imageFilterMMXon();
	}

	switch (op % 30) {
		case 0:
			SDL_imageFilterAdd(s1, s2, d, len);
			break;
		case 1:
			SDL_imageFilterMean(s1, s2, d, len);
			break;
		case 2:
			SDL_imageFilterSub(s1, s2, d, len);
			break;
		case 3:
			SDL_imageFilterAbsDiff(s1, s2, d, len);
			break;
		case 4:
			SDL_imageFilterMult(s1, s2, d, len);
			break;
		case 5:
			SDL_imageFilterMultNor(s1, s2, d, len);
			break;
		case 6:
			SDL_imageFilterMultDivby2(s1, s2, d, len);
			break;
		case 7:
			SDL_imageFilterMultDivby4(s1, s2, d, len);
			break;
		case 8:
			SDL_imageFilterBitAnd(s1, s2, d, len);
			break;
		case 9:
			SDL_imageFilterBitOr(s1, s2, d, len);
			break;
		case 10:
			SDL_imageFilterDiv(s1, s2, d, len);
			break;
		case 11:
			SDL_imageFilterBitNegation(s1, d, len);
			break;
		case 12:
			SDL_imageFilterAddByte(s1, d, len, p1);
			break;
		case 13:
			SDL_imageFilterAddUint(s1, d, len, (unsigned int)ci[0]);
			break;
		case 14:
			SDL_imageFilterAddByteToHalf(s1, d, len, p1);
			break;
		case 15:
			SDL_imageFilterSubByte(s1, d, len, p1);
			break;
		case 16:
			SDL_imageFilterSubUint(s1, d, len, (unsigned int)ci[0]);
			break;
		case 17:
			SDL_imageFilterShiftRight(s1, d, len, p1);
			break;
		case 18:
			SDL_imageFilterShiftRightUint(s1, d, len, p1);
			break;
		case 19:
			SDL_imageFilterMultByByte(s1, d, len, p1);
			break;
		case 20:
			SDL_imageFilterShiftRightAndMultByByte(s1, d, len, p1, p2);
			break;
		case 21:
			SDL_imageFilterShiftLeftByte(s1, d, len, p1);
			break;
		case 22:
			SDL_imageFilterShiftLeftUint(s1, d, len, p1);
			break;
		case 23:
			SDL_imageFilterShiftLeft(s1, d, len, p1);
			break;
		case 24:
			SDL_imageFilterBinarizeUsingThreshold(s1, d, len, p1);
			break;
		case 25:
			SDL_imageFilterClipToRange(s1, d, len, p1, p2);
			break;
		case 26:
			SDL_imageFilterNormalizeLinear(s1, d, len, ci[0], ci[1], ci[2], ci[3]);
			break;
		case 27:
			SDL_imageFilterNormalizeLinear(s1, d, len, ci[0] % 256, ci[1] % 256, ci[2] % 256, ci[3] % 256);
			break;
		case 28:
			SDL_imageFilterAdd(s1, s1, s1, len);
			break; /* in-place (aliasing) */
		default:
			SDL_imageFilterShiftRight(s1, s1, len, p1);
			break;
	}

	free(s1);
	free(s2);
	free(d);
	return 0;
}
