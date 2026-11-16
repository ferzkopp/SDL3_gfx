/*
 * Unit tests for SDL3_imageFilter: every byte-wise filter is checked against the formula documented
 * in SDL3_imageFilter.h, for all lengths 0..67 and unaligned buffer offsets, including a canary
 * check that no byte beyond 'length' is written.
 *
 * The ...Uint and NormalizeLinear functions are checked against their documented definitions in\n * SDL3_imageFilter.c. SDL_imageFilterMMXoff/on are kept for compatibility and have no effect.
 */
#include "testutils.h"

#include "SDL3_imageFilter.h"

#define MAXLEN 67
#define PAD    8
#define CANARY 0xA5

typedef int (*Filter2)(unsigned char *, unsigned char *, unsigned char *, unsigned int);
typedef unsigned char (*Ref2)(unsigned char, unsigned char);

static unsigned char sat(int v)
{
	return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

static unsigned char ref_add(unsigned char a, unsigned char b)
{ return sat(a + b); }
static unsigned char ref_mean(unsigned char a, unsigned char b)
{ return (unsigned char)(a / 2 + b / 2); }
static unsigned char ref_sub(unsigned char a, unsigned char b)
{ return sat(a - b); }
static unsigned char ref_absdiff(unsigned char a, unsigned char b)
{ return (unsigned char)(a > b ? a - b : b - a); }
static unsigned char ref_mult(unsigned char a, unsigned char b)
{ return sat(a * b); }
static unsigned char ref_multnor(unsigned char a, unsigned char b)
{ return (unsigned char)(a * b); }
static unsigned char ref_multinv(unsigned char a, unsigned char b)
{ return (unsigned char)(255 - ((255 - a) * (255 - b) + 127) / 255); }
static unsigned char ref_multdiv2(unsigned char a, unsigned char b)
{ return sat((a / 2) * b); }
static unsigned char ref_multdiv4(unsigned char a, unsigned char b)
{ return sat((a / 2) * (b / 2)); }
static unsigned char ref_and(unsigned char a, unsigned char b)
{ return (unsigned char)(a & b); }
static unsigned char ref_or(unsigned char a, unsigned char b)
{ return (unsigned char)(a | b); }
static unsigned char ref_div(unsigned char a, unsigned char b)
{ return (unsigned char)(b == 0 ? 255 : a / b); }

static void fill_random(unsigned char *buf, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		buf[i] = SDLTest_RandomUint8();
	}
	/* Make sure the extreme values are always present. */
	if (n > 2) {
		buf[0] = 0;
		buf[1] = 255;
	}
}

static void SDLCALL setup(void **arg)
{
	(void)arg;
	SDL_imageFilterMMXoff();
}

static void SDLCALL teardown(void *arg)
{
	(void)arg;
	SDL_imageFilterMMXon();
}

static int check_binary(const char *name, Filter2 f, Ref2 ref)
{
	unsigned char s1[MAXLEN + PAD], s2[MAXLEN + PAD], d[MAXLEN + 2 * PAD];
	unsigned int len, off, i;
	int errors = 0;

	for (len = 0; len <= MAXLEN; len++) {
		for (off = 0; off < 4; off++) {
			unsigned char *dst = d + PAD + off;
			fill_random(s1 + off, (int)len);
			fill_random(s2 + off, (int)len);
			SDL_memset(d, CANARY, sizeof(d));
			if (f(s1 + off, s2 + off, dst, len) != 0) {
				errors++;
				continue;
			}
			for (i = 0; i < len; i++) {
				if (dst[i] != ref(s1[off + i], s2[off + i])) {
					if (errors == 0) {
						SDLTest_Log("%s: first mismatch len=%u off=%u i=%u: f(%u,%u)=%u expected %u", name, len, off, i,
							s1[off + i], s2[off + i], dst[i], ref(s1[off + i], s2[off + i]));
					}
					errors++;
					break;
				}
			}
			if (dst[-1] != CANARY || dst[len] != CANARY) {
				if (errors == 0) {
					SDLTest_Log("%s: canary overwritten len=%u off=%u", name, len, off);
				}
				errors++;
			}
		}
	}
	SDLTest_AssertCheck(errors == 0, "%s matches the documented formula for lengths 0..%d (errors: %d)", name, MAXLEN, errors);
	return errors;
}

#define BINARY_TEST(fn, ref)                         \
	static int test_##fn(void *arg)                  \
	{                                                \
		(void)arg;                                   \
		check_binary(#fn, SDL_imageFilter##fn, ref); \
		return TEST_COMPLETED;                       \
	}                                                \
	static const SDLTest_TestCaseReference test_##fn##_ref = {test_##fn, "imageFilter" #fn, "SDL_imageFilter" #fn, TEST_ENABLED};

BINARY_TEST(Add, ref_add)
BINARY_TEST(Mean, ref_mean)
BINARY_TEST(Sub, ref_sub)
BINARY_TEST(AbsDiff, ref_absdiff)
BINARY_TEST(Mult, ref_mult)
BINARY_TEST(MultNor, ref_multnor)
BINARY_TEST(MultUnbound, ref_multnor)
BINARY_TEST(MultInv, ref_multinv)
BINARY_TEST(MultDivby2, ref_multdiv2)
BINARY_TEST(MultDivby4, ref_multdiv4)
BINARY_TEST(BitAnd, ref_and)
BINARY_TEST(BitOr, ref_or)
BINARY_TEST(Div, ref_div)

/* Unary filters with byte parameters: op selects the documented formula. */
enum UnaryOp {
	U_NEG,
	U_ADDBYTE,
	U_ADDBYTETOHALF,
	U_SUBBYTE,
	U_SHR,
	U_MULTBYBYTE,
	U_SHRMULT,
	U_SHLBYTE,
	U_SHL,
	U_BINARIZE,
	U_CLIP
};

static unsigned char ref_unary(int op, unsigned char s, unsigned char p1, unsigned char p2)
{
	switch (op) {
		case U_NEG:
			return (unsigned char)~s;
		case U_ADDBYTE:
			return sat(s + p1);
		case U_ADDBYTETOHALF:
			return sat(s / 2 + p1);
		case U_SUBBYTE:
			return sat(s - p1);
		case U_SHR:
			return (unsigned char)(s >> p1);
		case U_MULTBYBYTE:
			return sat(s * p1);
		case U_SHRMULT:
			return sat((s >> p1) * p2);
		case U_SHLBYTE:
			return (unsigned char)(s << p1);
		case U_SHL:
			return sat(s << p1);
		case U_BINARIZE:
			return (unsigned char)(s >= p1 ? 255 : 0);
		case U_CLIP:
			return (unsigned char)(s < p1 ? p1 : (s > p2 ? p2 : s));
		default:
			return 0;
	}
}

static int call_unary(int op, unsigned char *s, unsigned char *d, unsigned int len, unsigned char p1, unsigned char p2)
{
	switch (op) {
		case U_NEG:
			return SDL_imageFilterBitNegation(s, d, len);
		case U_ADDBYTE:
			return SDL_imageFilterAddByte(s, d, len, p1);
		case U_ADDBYTETOHALF:
			return SDL_imageFilterAddByteToHalf(s, d, len, p1);
		case U_SUBBYTE:
			return SDL_imageFilterSubByte(s, d, len, p1);
		case U_SHR:
			return SDL_imageFilterShiftRight(s, d, len, p1);
		case U_MULTBYBYTE:
			return SDL_imageFilterMultByByte(s, d, len, p1);
		case U_SHRMULT:
			return SDL_imageFilterShiftRightAndMultByByte(s, d, len, p1, p2);
		case U_SHLBYTE:
			return SDL_imageFilterShiftLeftByte(s, d, len, p1);
		case U_SHL:
			return SDL_imageFilterShiftLeft(s, d, len, p1);
		case U_BINARIZE:
			return SDL_imageFilterBinarizeUsingThreshold(s, d, len, p1);
		case U_CLIP:
			return SDL_imageFilterClipToRange(s, d, len, p1, p2);
		default:
			return -1;
	}
}

static int check_unary(const char *name, int op, const unsigned char (*params)[2], int nparams)
{
	unsigned char s[MAXLEN + PAD], d[MAXLEN + 2 * PAD];
	unsigned int len, off, i;
	int p, errors = 0;

	for (p = 0; p < nparams; p++) {
		for (len = 0; len <= MAXLEN; len++) {
			off = len % 4;
			{
				unsigned char *dst = d + PAD + off;
				fill_random(s + off, (int)len);
				SDL_memset(d, CANARY, sizeof(d));
				if (call_unary(op, s + off, dst, len, params[p][0], params[p][1]) != 0) {
					errors++;
					continue;
				}
				for (i = 0; i < len; i++) {
					if (dst[i] != ref_unary(op, s[off + i], params[p][0], params[p][1])) {
						errors++;
						break;
					}
				}
				if (dst[-1] != CANARY || dst[len] != CANARY) {
					errors++;
				}
			}
		}
	}
	SDLTest_AssertCheck(errors == 0, "%s matches the documented formula (errors: %d)", name, errors);
	return errors;
}

static const unsigned char p_none[][2] = {{0, 0}};
static const unsigned char p_byte[][2] = {{0, 0}, {1, 0}, {7, 0}, {100, 0}, {255, 0}};
static const unsigned char p_shift[][2] = {{0, 0}, {1, 0}, {3, 0}, {7, 0}};
static const unsigned char p_shrmult[][2] = {{0, 3}, {1, 2}, {4, 17}, {7, 255}};
static const unsigned char p_clip[][2] = {{0, 255}, {0, 25}, {10, 200}, {100, 100}, {0, 0}}; /* {0, 25}: IMGF-005 */

#define UNARY_TEST(fn, op, params)                                \
	static int test_##fn(void *arg)                               \
	{                                                             \
		(void)arg;                                                \
		check_unary(#fn, op, params, (int)SDL_arraysize(params)); \
		return TEST_COMPLETED;                                    \
	}                                                             \
	static const SDLTest_TestCaseReference test_##fn##_ref = {test_##fn, "imageFilter" #fn, "SDL_imageFilter" #fn, TEST_ENABLED};

UNARY_TEST(BitNegation, U_NEG, p_none)
UNARY_TEST(AddByte, U_ADDBYTE, p_byte)
UNARY_TEST(AddByteToHalf, U_ADDBYTETOHALF, p_byte)
UNARY_TEST(SubByte, U_SUBBYTE, p_byte)
UNARY_TEST(ShiftRight, U_SHR, p_shift)
UNARY_TEST(MultByByte, U_MULTBYBYTE, p_byte)
UNARY_TEST(ShiftRightAndMultByByte, U_SHRMULT, p_shrmult)
UNARY_TEST(ShiftLeftByte, U_SHLBYTE, p_shift)
UNARY_TEST(ShiftLeft, U_SHL, p_shift)
UNARY_TEST(BinarizeUsingThreshold, U_BINARIZE, p_byte)
UNARY_TEST(ClipToRange, U_CLIP, p_clip)

/* Parameter validation: NULL buffers return -1, length 0 returns 0 without writing. */
static int test_parameters(void *arg)
{
	unsigned char a[16], b[16], d[16];
	(void)arg;
	SDL_memset(d, CANARY, sizeof(d));
	SDLTest_AssertCheck(SDL_imageFilterAdd(NULL, b, d, 16) == -1, "Add(Src1=NULL) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterAdd(a, NULL, d, 16) == -1, "Add(Src2=NULL) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterAdd(a, b, NULL, 16) == -1, "Add(Dest=NULL) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterAddByte(NULL, d, 16, 1) == -1, "AddByte(Src1=NULL) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterNormalizeLinear(a, NULL, 16, 0, 255, 0, 255) == -1, "NormalizeLinear(Dest=NULL) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterAdd(a, b, d, 0) == 0, "Add(length=0) returns 0");
	SDLTest_AssertCheck(d[0] == CANARY, "length=0 did not write");
	SDLTest_AssertCheck(SDL_imageFilterShiftRight(a, d, 16, 9) == -1, "ShiftRight(N=9) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterShiftLeft(a, d, 16, 9) == -1, "ShiftLeft(N=9) returns -1");
	return TEST_COMPLETED;
}

/* Byte-wise constant from a 32-bit value: byte i uses byte (i % 4) of C, least significant first. */
static unsigned char uint_byte(unsigned int c, unsigned int i)
{
	return (unsigned char)((c >> (8 * (i & 3))) & 0xff);
}

/* Reference for the ...Uint shifts: native-endian 32-bit words, partial tail word zero padded. */
static void ref_shift_words(const unsigned char *s, unsigned char *d, unsigned int len, unsigned int n, int left)
{
	unsigned int i;
	for (i = 0; i < len; i += 4) {
		Uint32 w = 0;
		unsigned int bytes = (len - i < 4) ? (len - i) : 4;
		SDL_memcpy(&w, s + i, bytes);
		w = (n >= 32) ? 0 : (left ? (w << n) : (w >> n));
		SDL_memcpy(d + i, &w, bytes);
	}
}

/* AddUint/SubUint/ShiftRightUint/ShiftLeftUint (IMGF-003): every byte incl. the tail is processed. */
static int test_uint_functions(void *arg)
{
	unsigned char s[MAXLEN + PAD], d[MAXLEN + 2 * PAD], r[MAXLEN + PAD];
	static const unsigned int consts[] = {0, 0x01020304u, 0xFF00FF80u, 0xFFFFFFFFu};
	static const unsigned char shifts[] = {0, 1, 7, 8, 13, 31, 32};
	unsigned int len, off, i, k;
	int errors = 0;
	(void)arg;
	for (len = 0; len <= MAXLEN; len++) {
		off = len % 4;
		fill_random(s + off, (int)len);
		for (k = 0; k < SDL_arraysize(consts); k++) {
			unsigned char *dst = d + PAD + off;
			SDL_memset(d, CANARY, sizeof(d));
			SDL_imageFilterAddUint(s + off, dst, len, consts[k]);
			for (i = 0; i < len; i++) {
				errors += dst[i] != sat(s[off + i] + uint_byte(consts[k], i));
			}
			errors += dst[-1] != CANARY || dst[len] != CANARY;
			SDL_memset(d, CANARY, sizeof(d));
			SDL_imageFilterSubUint(s + off, dst, len, consts[k]);
			for (i = 0; i < len; i++) {
				errors += dst[i] != sat(s[off + i] - uint_byte(consts[k], i));
			}
			errors += dst[-1] != CANARY || dst[len] != CANARY;
		}
		for (k = 0; k < SDL_arraysize(shifts); k++) {
			unsigned char *dst = d + PAD + off;
			int left;
			for (left = 0; left < 2; left++) {
				SDL_memset(d, CANARY, sizeof(d));
				if (left) {
					SDL_imageFilterShiftLeftUint(s + off, dst, len, shifts[k]);
				} else {
					SDL_imageFilterShiftRightUint(s + off, dst, len, shifts[k]);
				}
				ref_shift_words(s + off, r, len, shifts[k], left);
				errors += SDL_memcmp(dst, r, len) != 0;
				errors += dst[-1] != CANARY || dst[len] != CANARY;
			}
		}
	}
	SDLTest_AssertCheck(errors == 0, "AddUint/SubUint/ShiftRightUint/ShiftLeftUint match their definitions (errors: %d)", errors);
	SDLTest_AssertCheck(SDL_imageFilterShiftRightUint(s, d, 4, 33) == -1, "ShiftRightUint(N=33) returns -1");
	SDLTest_AssertCheck(SDL_imageFilterShiftLeftUint(s, d, 4, 33) == -1, "ShiftLeftUint(N=33) returns -1");
	return TEST_COMPLETED;
}

/* NormalizeLinear (IMGF-004): exact scale factor, Cmax == Cmin rejected. */
static int test_normalize_linear(void *arg)
{
	static const int params[][4] = {{0, 255, 0, 255}, {50, 150, 0, 255}, {0, 100, 255, 0}, {200, 100, 10, 20}, {-1000, 1000, 0, 255},
		{0, 1, 0, 2147483647}};
	unsigned char s[256], d[256];
	unsigned int i, k;
	int errors = 0;
	(void)arg;
	for (i = 0; i < 256; i++) {
		s[i] = (unsigned char)i;
	}
	for (k = 0; k < SDL_arraysize(params); k++) {
		const int cmin = params[k][0], cmax = params[k][1], nmin = params[k][2], nmax = params[k][3];
		SDLTest_AssertCheck(SDL_imageFilterNormalizeLinear(s, d, 256, cmin, cmax, nmin, nmax) == 0, "NormalizeLinear(%d,%d,%d,%d)", cmin, cmax,
			nmin, nmax);
		for (i = 0; i < 256; i++) {
			Sint64 v = ((Sint64)nmax - nmin) * ((Sint64)i - cmin) / ((Sint64)cmax - cmin) + nmin;
			errors += d[i] != (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v));
		}
	}
	SDLTest_AssertCheck(errors == 0, "NormalizeLinear matches the exact formula (errors: %d)", errors);
	SDLTest_AssertCheck(SDL_imageFilterNormalizeLinear(s, d, 256, 7, 7, 0, 255) == -1, "NormalizeLinear with Cmax == Cmin returns -1");
	/* 50..150 -> 0..255: 100 maps to 127 (the former integer factor 255/100 = 2 gave 100) */
	SDL_imageFilterNormalizeLinear(s, d, 256, 50, 150, 0, 255);
	SDLTest_AssertCheck(d[100] == 127, "NormalizeLinear(50,150,0,255) maps 100 to %u (expected 127)", d[100]);
	return TEST_COMPLETED;
}
/* The default (MMX-detecting) path must produce the same result as the C path (finding IMGF-002). */
static int test_default_path_matches_c(void *arg)
{
	unsigned char s1[64], s2[64], dc[64], dd[64];
	(void)arg;
	fill_random(s1, 64);
	fill_random(s2, 64);
	SDL_imageFilterMMXoff();
	SDL_imageFilterAdd(s1, s2, dc, 64);
	SDL_imageFilterMMXon();
	SDL_imageFilterAdd(s1, s2, dd, 64);
	SDLTest_AssertCheck(SDL_memcmp(dc, dd, 64) == 0, "Default path matches the C path");
	return TEST_COMPLETED;
}

static const SDLTest_TestCaseReference test_parameters_ref = {test_parameters, "imageFilterParameters", "Parameter validation", TEST_ENABLED};
static const SDLTest_TestCaseReference test_uint_ref = {test_uint_functions, "imageFilterUint", "...Uint functions (IMGF-003)", TEST_ENABLED};
static const SDLTest_TestCaseReference test_normalize_ref = {test_normalize_linear, "imageFilterNormalizeLinear", "NormalizeLinear (IMGF-004)", TEST_ENABLED};
static const SDLTest_TestCaseReference test_default_path_ref = {test_default_path_matches_c, "imageFilterDefaultPath", "Default path == C path (IMGF-002)", TEST_ENABLED};

static const SDLTest_TestCaseReference *imageFilterTests[] = {
	&test_Add_ref, &test_Mean_ref, &test_Sub_ref, &test_AbsDiff_ref, &test_Mult_ref, &test_MultNor_ref, &test_MultUnbound_ref, &test_MultInv_ref, &test_MultDivby2_ref,
	&test_MultDivby4_ref, &test_BitAnd_ref, &test_BitOr_ref, &test_Div_ref, &test_BitNegation_ref, &test_AddByte_ref,
	&test_AddByteToHalf_ref, &test_SubByte_ref, &test_ShiftRight_ref, &test_MultByByte_ref, &test_ShiftRightAndMultByByte_ref,
	&test_ShiftLeftByte_ref, &test_ShiftLeft_ref, &test_BinarizeUsingThreshold_ref, &test_ClipToRange_ref, &test_parameters_ref,
	&test_uint_ref, &test_normalize_ref, &test_default_path_ref, NULL};

SDLTest_TestSuiteReference imageFilterTestSuite = {"ImageFilter", setup, imageFilterTests, teardown};
