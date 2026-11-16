/*

SDL3_imageFilter.c: byte-image "filter" routines

Copyright (C) 2012-2026  Andreas Schiffler
Copyright (C) 2013  Sylvain Beucler

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.

Andreas Schiffler -- aschiffler at ferzkopp dot net

*/

/*!
\file SDL3_imageFilter.c
\brief Implementation of the image filters (portable C).
*/

/*

Portable C implementation of all filters. The former inline MMX assembly (32-bit x86 only) was
removed in 1.1.0: modern compilers auto-vectorize these loops, and the MMX dispatch was broken on
x64 builds (only the last length % 8 bytes were processed).

All functions accept Dest == Src1 (or Dest == Src2) for in-place operation.

*/

#include <string.h>

#include <SDL3/SDL.h>

#include "SDL3_imageFilter.h"

/* ------ Helpers ----- */

/* Saturation written as unsigned min/max so that compilers vectorize the filter loops. */
static unsigned char saturate255(unsigned int v)
{
	return (unsigned char)((v < 255u) ? v : 255u);
}

/* a - b clamped at 0 */
static unsigned char subtract0(unsigned int a, unsigned int b)
{
	return (unsigned char)((a > b) ? (a - b) : 0u);
}

static unsigned char saturate(Sint64 v)
{
	return (unsigned char)((v < 0) ? 0 : ((v > 255) ? 255 : v));
}

/* ------ MMX control (deprecated, kept for API compatibility) ----- */

/*!
\brief MMX detection routine (deprecated).

The filters no longer contain MMX code; they are implemented in portable C.

\returns Always 0.
*/
int SDL_imageFilterMMXdetect(void)
{
	return (0);
}

/*!
\brief Disable MMX code (deprecated; no effect).
*/
void SDL_imageFilterMMXoff(void)
{
}

/*!
\brief Enable MMX code (deprecated; no effect).
*/
void SDL_imageFilterMMXon(void)
{
}

/* ------ Two-source filters ----- */

/*!
\brief Filter using Add: D = saturation255(S1 + S2)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterAdd(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] + Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using Mean: D = S1/2 + S2/2

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMean(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)((Src1[i] >> 1) + (Src2[i] >> 1));
	}
	return (0);
}

/*!
\brief Filter using Sub: D = saturation0(S1 - S2)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterSub(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = subtract0(Src1[i], Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using AbsDiff: D = | S1 - S2 |

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterAbsDiff(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		/* No abs(): MSVC 19.51 /O2 drops it when vectorizing this loop (TOOL-001). */
		Dest[i] = (unsigned char)((Src1[i] > Src2[i]) ? (Src1[i] - Src2[i]) : (Src2[i] - Src1[i]));
	}
	return (0);
}

/*!
\brief Filter using Mult: D = saturation255(S1 * S2)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMult(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] * Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using MultNor: D = S1 * S2 (modulo 256, no saturation)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultNor(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(Src1[i] * Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using MultUnbound: D = S1 * S2 (modulo 256, no saturation)

Same as SDL_imageFilterMultNor; the name was introduced by the SDL3 port (sabdul-khabir/SDL3_gfx 1.0.1).

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultUnbound(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	return SDL_imageFilterMultNor(Src1, Src2, Dest, length);
}

/*!
\brief Filter using MultInv ("screen"): D = 255 - (255 - S1) * (255 - S2) / 255 (rounded)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultInv(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(255u - ((255u - Src1[i]) * (255u - Src2[i]) + 127u) / 255u);
	}
	return (0);
}

/*!\brief Filter using MultDivby2: D = saturation255(S1/2 * S2)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultDivby2(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)(Src1[i] >> 1) * Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using MultDivby4: D = saturation255(S1/2 * S2/2)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultDivby4(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)(Src1[i] >> 1) * (unsigned int)(Src2[i] >> 1));
	}
	return (0);
}

/*!
\brief Filter using BitAnd: D = S1 & S2

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterBitAnd(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(Src1[i] & Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using BitOr: D = S1 | S2

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterBitOr(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(Src1[i] | Src2[i]);
	}
	return (0);
}

/*!
\brief Filter using Div: D = S1 / S2 (integer division; 255 where S2 is 0)

\param Src1 Pointer to the start of the first source byte array (S1).
\param Src2 Pointer to the start of the second source byte array (S2).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterDiv(unsigned char *Src1, unsigned char *Src2, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Src2 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)((Src2[i] == 0) ? 255 : (Src1[i] / Src2[i]));
	}
	return (0);
}

/* ------ Single-source filters ----- */

/*!
\brief Filter using BitNegation: D = !S (bitwise complement)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterBitNegation(unsigned char *Src1, unsigned char *Dest, unsigned int length)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)~Src1[i];
	}
	return (0);
}

/*!
\brief Filter using AddByte: D = saturation255(S + C)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param C Constant value to add (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterAddByte(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] + C);
	}
	return (0);
}

/*!
\brief Filter using AddUint: D = saturation255((S[i] + Cs[i % 4])

The constant is applied byte-wise: byte i of the image is added to byte (i % 4) of C, where byte 0 is
the least significant byte of C.

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param C Constant to add (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterAddUint(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned int C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] + ((C >> (8 * (i & 3))) & 0xffu));
	}
	return (0);
}

/*!
\brief Filter using AddByteToHalf: D = saturation255(S/2 + C)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param C Constant to add (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterAddByteToHalf(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)(Src1[i] >> 1) + C);
	}
	return (0);
}

/*!
\brief Filter using SubByte: D = saturation0(S - C)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.
\param C Constant to subtract (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterSubByte(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = subtract0(Src1[i], C);
	}
	return (0);
}

/*!
\brief Filter using SubUint: D = saturation0(S[i] - Cs[i % 4])

The constant is applied byte-wise: byte (i % 4) of C, where byte 0 is the least significant byte of
C, is subtracted from byte i of the image.

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param C Constant to subtract (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterSubUint(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned int C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = subtract0(Src1[i], (C >> (8 * (i & 3))) & 0xffu);
	}
	return (0);
}

/*!
\brief Filter using ShiftRight: D = saturation0(S >> N)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param N Number of bit-positions to shift (N). Valid range is 0 to 8.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftRight(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL) || (N > 8)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(Src1[i] >> N);
	}
	return (0);
}

/* Applies a shift to 32-bit words in native byte order. A trailing partial word (length % 4 bytes) is
   treated as a word padded with zero bytes. Buffers need no particular alignment. */
static void shift_words(const unsigned char *src, unsigned char *dst, unsigned int length, unsigned char n, int left)
{
	unsigned int i;
	for (i = 0; i < length; i += 4) {
		Uint32 w = 0;
		unsigned int bytes = (length - i < 4) ? (length - i) : 4;
		SDL_memcpy(&w, src + i, bytes);
		if (n >= 32) {
			w = 0;
		} else {
			w = left ? (w << n) : (w >> n);
		}
		SDL_memcpy(dst + i, &w, bytes);
	}
}

/*!
\brief Filter using ShiftRightUint: D = (uint)S >> N

The image is processed as 32-bit words in native byte order; a trailing partial word is padded with
zero bytes.

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param N Number of bit-positions to shift (N). Valid range is 0 to 32.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftRightUint(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N)
{
	if ((Src1 == NULL) || (Dest == NULL) || (N > 32)) {
		return (-1);
	}
	shift_words(Src1, Dest, length, N, 0);
	return (0);
}

/*!
\brief Filter using MultByByte: D = saturation255(S * C)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.
\param C Constant to multiply with (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterMultByByte(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] * C);
	}
	return (0);
}

/*!
\brief Filter using ShiftRightAndMultByByte: D = saturation255((S >> N) * C)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param N Number of bit-positions to shift (N). Valid range is 0 to 8.
\param C Constant to multiply with (C).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftRightAndMultByByte(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N,
	unsigned char C)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL) || (N > 8)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)(Src1[i] >> N) * C);
	}
	return (0);
}

/*!
\brief Filter using ShiftLeftByte: D = (S << N) (modulo 256, no saturation)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source arrays.
\param N Number of bit-positions to shift (N). Valid range is 0 to 8.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftLeftByte(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL) || (N > 8)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)(Src1[i] << N);
	}
	return (0);
}

/*!
\brief Filter using ShiftLeftUint: D = ((uint)S << N)

The image is processed as 32-bit words in native byte order; a trailing partial word is padded with
zero bytes.

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param N Number of bit-positions to shift (N). Valid range is 0 to 32.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftLeftUint(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N)
{
	if ((Src1 == NULL) || (Dest == NULL) || (N > 32)) {
		return (-1);
	}
	shift_words(Src1, Dest, length, N, 1);
	return (0);
}

/*!
\brief Filter using ShiftLeft: D = saturation255(S << N)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param N Number of bit-positions to shift (N). Valid range is 0 to 8.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterShiftLeft(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char N)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL) || (N > 8)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate255((unsigned int)Src1[i] << N);
	}
	return (0);
}

/*!
\brief Filter using BinarizeUsingThreshold: D = (S >= T) ? 255:0

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param T The threshold boundary (inclusive).

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterBinarizeUsingThreshold(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char T)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = (unsigned char)((Src1[i] >= T) ? 255 : 0);
	}
	return (0);
}

/*!
\brief Filter using ClipToRange: D = (S < Tmin) ? Tmin : ((S > Tmax) ? Tmax : S)

\param Src1 Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param Tmin Lower (inclusive) boundary of the clipping range.
\param Tmax Upper (inclusive) boundary of the clipping range.

\return Returns 0 for success or -1 for error.
*/
int SDL_imageFilterClipToRange(unsigned char *Src1, unsigned char *Dest, unsigned int length, unsigned char Tmin,
	unsigned char Tmax)
{
	unsigned int i;
	if ((Src1 == NULL) || (Dest == NULL)) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		unsigned char s = Src1[i];
		Dest[i] = (s < Tmin) ? Tmin : ((s > Tmax) ? Tmax : s);
	}
	return (0);
}

/*!
\brief Filter using NormalizeLinear: D = saturation255((Nmax - Nmin)/(Cmax - Cmin)*(S - Cmin) + Nmin)

The scale factor is applied exactly (64-bit integer arithmetic, result truncated toward zero).

\param Src Pointer to the start of the source byte array (S).
\param Dest Pointer to the start of the destination byte array (D).
\param length The number of bytes in the source array.
\param Cmin Normalization constant (Cmin).
\param Cmax Normalization constant (Cmax); must differ from Cmin.
\param Nmin Normalization constant (Nmin).
\param Nmax Normalization constant (Nmax).

\return Returns 0 for success or -1 for error (including Cmax == Cmin).
*/
int SDL_imageFilterNormalizeLinear(unsigned char *Src, unsigned char *Dest, unsigned int length, int Cmin, int Cmax,
	int Nmin, int Nmax)
{
	unsigned int i;
	Sint64 dC, dN;
	if ((Src == NULL) || (Dest == NULL)) {
		return (-1);
	}
	dC = (Sint64)Cmax - (Sint64)Cmin;
	dN = (Sint64)Nmax - (Sint64)Nmin;
	if (dC == 0) {
		return (-1);
	}
	for (i = 0; i < length; i++) {
		Dest[i] = saturate(dN * ((Sint64)Src[i] - (Sint64)Cmin) / dC + (Sint64)Nmin);
	}
	return (0);
}
