/* 

SDL3_gfxPrimitives.c: graphics primitives for SDL3 renderers

Copyright (C) 2012-2014  Andreas Schiffler

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
\file SDL3_gfxPrimitives.c
\brief Implementation of the graphics primitives.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "SDL3_gfxPrimitives.h"
#include "SDL3_rotozoom.h"
#include "SDL3_gfxPrimitives_font.h"

/* Decoding of the Uint32 color argument of the ...Color functions: 0xRRGGBBAA as documented.
   Up to version 1.0.4 the color was decoded from its bytes in memory order, i.e. 0xAABBGGRR on
   little-endian machines (finding API-001); define SDL3_GFX_LEGACY_COLOR_BYTE_ORDER (CMake option
   SDL3GFX_LEGACY_COLOR_BYTE_ORDER) to restore that behavior. */
#ifdef SDL3_GFX_LEGACY_COLOR_BYTE_ORDER
#define GFX_COLOR_BYTE(color, i) (((const Uint8 *)&(color))[i])
#define GFX_COLOR_R(color)       GFX_COLOR_BYTE(color, 0)
#define GFX_COLOR_G(color)       GFX_COLOR_BYTE(color, 1)
#define GFX_COLOR_B(color)       GFX_COLOR_BYTE(color, 2)
#define GFX_COLOR_A(color)       GFX_COLOR_BYTE(color, 3)
#else
#define GFX_COLOR_R(color) ((Uint8)((color) >> 24)) /*!< Red component of a 0xRRGGBBAA color. */
#define GFX_COLOR_G(color) ((Uint8)((color) >> 16)) /*!< Green component of a 0xRRGGBBAA color. */
#define GFX_COLOR_B(color) ((Uint8)((color) >> 8)) /*!< Blue component of a 0xRRGGBBAA color. */
#define GFX_COLOR_A(color) ((Uint8)(color)) /*!< Alpha component of a 0xRRGGBBAA color. */
#endif

/*! \brief Maps the bool result of an SDL3 function to the 0 (success) / -1 (error) convention of this API. */
#define GFX_RESULT(call) ((call) ? 0 : -1)

/* ---- Structures */

/*!
\brief The structure passed to the internal Bresenham iterator.
*/
typedef struct {
	Sint16 x, y;
	int dx, dy, s1, s2, swapdir, error;
	Uint32 count;
} SDL3_gfxBresenhamIterator;

/*!
\brief The structure passed to the internal Murphy iterator.
*/
typedef struct {
	SDL_Renderer *renderer;
	int u, v; /* delta x , delta y */
	int ku, kt, kv, kd; /* loop constants */
	int oct2;
	int quad4;
	Sint16 last1x, last1y, last2x, last2y, first1x, first1y, first2x, first2y, tempx, tempy;
} SDL3_gfxMurphyIterator;

/* ---- Pixel */

/*!
\brief Internal function to draw a line; a single point is drawn as a point.

SDL3's software renderer draws nothing for a zero-length line when blending (PRIM-015).
*/
static int _gfxDrawLine(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2)
{
	if ((x1 == x2) && (y1 == y2)) {
		return GFX_RESULT(SDL_RenderPoint(renderer, x1, y1));
	}
	return GFX_RESULT(SDL_RenderLine(renderer, x1, y1, x2, y2));
}
/*!
\brief Draw pixel with blending enabled if a<255.

\param renderer The renderer to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param color The color value of the pixel to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int pixelColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Uint32 color)
{
	return pixelRGBA(renderer, x, y, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw pixel with blending enabled if a<255.

\param renderer The renderer to draw on.
\param x X (horizontal) coordinate of the pixel.
\param y Y (vertical) coordinate of the pixel.
\param r The red color value of the pixel to draw. 
\param g The green color value of the pixel to draw.
\param b The blue color value of the pixel to draw.
\param a The alpha value of the pixel to draw.

\returns Returns 0 on success, -1 on failure.
*/
int pixelRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= GFX_RESULT(SDL_RenderPoint(renderer, x, y));
	return result;
}

/* ---- Hline */

/*!
\brief Draw horizontal line with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int hlineColor(SDL_Renderer *renderer, Sint16 x1, Sint16 x2, Sint16 y, Uint32 color)
{
	return hlineRGBA(renderer, x1, x2, y, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw horizontal line with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param r The red value of the line to draw. 
\param g The green value of the line to draw. 
\param b The blue value of the line to draw. 
\param a The alpha value of the line to draw. 

\returns Returns 0 on success, -1 on failure.
*/
int hlineRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 x2, Sint16 y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= _gfxDrawLine(renderer, x1, y, x2, y);
	return result;
}

/* ---- Vline */

/*!
\brief Draw vertical line with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the points of the line.
\param y1 Y coordinate of the first point (i.e. top) of the line.
\param y2 Y coordinate of the second point (i.e. bottom) of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int vlineColor(SDL_Renderer *renderer, Sint16 x, Sint16 y1, Sint16 y2, Uint32 color)
{
	return vlineRGBA(renderer, x, y1, y2, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw vertical line with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the points of the line.
\param y1 Y coordinate of the first point (i.e. top) of the line.
\param y2 Y coordinate of the second point (i.e. bottom) of the line.
\param r The red value of the line to draw. 
\param g The green value of the line to draw. 
\param b The blue value of the line to draw. 
\param a The alpha value of the line to draw. 

\returns Returns 0 on success, -1 on failure.
*/
int vlineRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y1, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= _gfxDrawLine(renderer, x, y1, x, y2);
	return result;
}

/* ---- Draw call batching (PERF-002) */

/*!
\brief Hint to control how anti-aliased pixels are drawn: "1" as SDL_RenderGeometry quads (one draw call per batch),
"0" as individual points. Default: quads for hardware renderers, points for the software renderer (where geometry
is slower and rounds differently). Mainly for testing.
*/
#define SDL3_GFX_HINT_AA_GEOMETRY "SDL3_GFX_AA_GEOMETRY"

#define GFX_BATCH_POINTS 256 /*!< Capacity of a batch for points. */
#define GFX_BATCH_RECTS  128 /*!< Capacity of a batch for spans (rectangles). */
#define GFX_BATCH_QUADS  64 /*!< Capacity of a batch for anti-aliased pixel quads. */

#define GFX_BATCH_NONE  0 /*!< Batch kind: empty. */
#define GFX_BATCH_POINT 1 /*!< Batch kind: points. */
#define GFX_BATCH_RECT  2 /*!< Batch kind: spans (rectangles). */
#define GFX_BATCH_QUAD  3 /*!< Batch kind: anti-aliased pixel quads (geometry). */
#define GFX_BATCH_PIXEL 4 /*!< Batch kind: anti-aliased pixels drawn immediately (software renderer). */

/*!
\brief Collects the points, spans and anti-aliased pixels of one primitive and draws them with few SDL calls.

Only one kind of item is pending at a time and the batch is flushed whenever the kind changes, so the
drawing order (which matters for overlapping blended pixels) is the same as with individual calls.
*/
typedef struct {
	SDL_Renderer *renderer;
	int result;
	Uint8 r, g, b, a;
	int kind;
	int count;
	int software; /* software renderer; -1: not determined yet */
	int geometry; /* AA pixels as quads; -1: not decided yet */
	int colorSet; /* draw color and blend mode of the primitive are set on the renderer */
	union {
		SDL_FPoint points[GFX_BATCH_POINTS];
		SDL_FRect rects[GFX_BATCH_RECTS];
		struct {
			float xy[GFX_BATCH_QUADS * 8];
			SDL_FColor colors[GFX_BATCH_QUADS * 4];
		} quads;
	} items;
} GFX_Batch;

/*!
\brief Internal function to start a batch for a primitive drawn in the given color.

Sets the draw color and blend mode like the unbatched primitives did.
*/
static void _gfxBatchBegin(GFX_Batch *batch, SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	batch->renderer = renderer;
	batch->r = r;
	batch->g = g;
	batch->b = b;
	batch->a = a;
	batch->kind = GFX_BATCH_NONE;
	batch->count = 0;
	batch->software = -1;
	batch->geometry = -1;
	batch->result = 0;
	batch->result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	batch->result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	batch->colorSet = 1;
}

/*!
\brief Internal function to determine (once per batch) whether the renderer is SDL's software renderer.
*/
static int _gfxBatchIsSoftware(GFX_Batch *batch)
{
	const char *name;

	if (batch->software < 0) {
		name = SDL_GetRendererName(batch->renderer);
		batch->software = (name != NULL) && (SDL_strcmp(name, SDL_SOFTWARE_RENDERER) == 0);
	}
	return batch->software;
}
/*!
\brief Internal function to draw the pending items of a batch.

Spans are drawn as one SDL_RenderFillRects call, except for horizontal spans on the software renderer
where drawing a line is much cheaper than SDL_RenderFillRect.
*/
static void _gfxBatchFlush(GFX_Batch *batch)
{
	int i, n;
	Uint16 indices[GFX_BATCH_QUADS * 6];

	if (batch->count == 0) {
		return;
	}
	if ((batch->kind != GFX_BATCH_QUAD) && !batch->colorSet) {
		batch->result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(batch->renderer, (batch->a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
		batch->result |= GFX_RESULT(SDL_SetRenderDrawColor(batch->renderer, batch->r, batch->g, batch->b, batch->a));
		batch->colorSet = 1;
	}
	switch (batch->kind) {
		case GFX_BATCH_POINT:
			batch->result |= GFX_RESULT(SDL_RenderPoints(batch->renderer, batch->items.points, batch->count));
			break;
		case GFX_BATCH_RECT:
			if (_gfxBatchIsSoftware(batch)) {
				/* All items have the same color, so drawing them in another order gives the same result */
				n = 0;
				for (i = 0; i < batch->count; i++) {
					const SDL_FRect *rect = &batch->items.rects[i];
					if (rect->h == 1.0f) {
						batch->result |= _gfxDrawLine(batch->renderer, (Sint16)rect->x, (Sint16)rect->y, (Sint16)(rect->x + rect->w - 1.0f), (Sint16)rect->y);
					} else {
						batch->items.rects[n++] = *rect;
					}
				}
				if (n > 0) {
					batch->result |= GFX_RESULT(SDL_RenderFillRects(batch->renderer, batch->items.rects, n));
				}
			} else {
				batch->result |= GFX_RESULT(SDL_RenderFillRects(batch->renderer, batch->items.rects, batch->count));
			}
			break;
		case GFX_BATCH_QUAD:
			for (i = 0; i < batch->count; i++) {
				indices[i * 6 + 0] = (Uint16)(i * 4 + 0);
				indices[i * 6 + 1] = (Uint16)(i * 4 + 1);
				indices[i * 6 + 2] = (Uint16)(i * 4 + 2);
				indices[i * 6 + 3] = (Uint16)(i * 4 + 0);
				indices[i * 6 + 4] = (Uint16)(i * 4 + 2);
				indices[i * 6 + 5] = (Uint16)(i * 4 + 3);
			}
			batch->result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(batch->renderer, SDL_BLENDMODE_BLEND));
			batch->colorSet = 0;
			batch->result |= GFX_RESULT(SDL_RenderGeometryRaw(batch->renderer, NULL, batch->items.quads.xy, 2 * sizeof(float), batch->items.quads.colors,
				sizeof(SDL_FColor), NULL, 0, batch->count * 4, indices, batch->count * 6, sizeof(Uint16)));
			break;
		default:
			break;
	}
	batch->count = 0;
}

/*!
\brief Internal function to flush a batch if it holds items of another kind or is full.
*/
static void _gfxBatchPrepare(GFX_Batch *batch, int kind, int capacity)
{
	if ((batch->kind != kind) || (batch->count >= capacity)) {
		_gfxBatchFlush(batch);
		batch->kind = kind;
	}
}

/*!
\brief Internal function to finish a batch.

\returns Returns 0 on success, -1 if any SDL call failed.
*/
static int _gfxBatchEnd(GFX_Batch *batch)
{
	_gfxBatchFlush(batch);
	return (batch->result);
}

/*!
\brief Internal function to add a point to a batch.
*/
static void _gfxBatchPoint(GFX_Batch *batch, Sint16 x, Sint16 y)
{
	_gfxBatchPrepare(batch, GFX_BATCH_POINT, GFX_BATCH_POINTS);
	batch->items.points[batch->count].x = (float)x;
	batch->items.points[batch->count].y = (float)y;
	batch->count++;
}

/*!
\brief Internal function to add a rectangle (left, top, width, height) to a batch.
*/
static void _gfxBatchRect(GFX_Batch *batch, int x, int y, int w, int h)
{
	_gfxBatchPrepare(batch, GFX_BATCH_RECT, GFX_BATCH_RECTS);
	batch->items.rects[batch->count].x = (float)x;
	batch->items.rects[batch->count].y = (float)y;
	batch->items.rects[batch->count].w = (float)w;
	batch->items.rects[batch->count].h = (float)h;
	batch->count++;
}

/*!
\brief Internal function to add a horizontal line (both endpoints included) to a batch.
*/
static void _gfxBatchHLine(GFX_Batch *batch, Sint16 x1, Sint16 x2, Sint16 y)
{
	if (x1 <= x2) {
		_gfxBatchRect(batch, x1, y, x2 - x1 + 1, 1);
	} else {
		_gfxBatchRect(batch, x2, y, x1 - x2 + 1, 1);
	}
}

/*!
\brief Internal function to add a vertical line (both endpoints included) to a batch.
*/
static void _gfxBatchVLine(GFX_Batch *batch, Sint16 x, Sint16 y1, Sint16 y2)
{
	if (y1 <= y2) {
		_gfxBatchRect(batch, x, y1, 1, y2 - y1 + 1);
	} else {
		_gfxBatchRect(batch, x, y2, 1, y1 - y2 + 1);
	}
}

/*!
\brief Internal function to decide whether anti-aliased pixels are drawn as geometry.
*/
static int _gfxBatchUseGeometry(GFX_Batch *batch)
{
	const char *hint = SDL_GetHint(SDL3_GFX_HINT_AA_GEOMETRY);

	if ((hint != NULL) && (*hint != '\0')) {
		return (SDL_atoi(hint) != 0);
	}
	return !_gfxBatchIsSoftware(batch);
}
/*!
\brief Internal function to add an anti-aliased pixel with the given alpha to a batch.

Hardware renderers get one colored quad per pixel; the software renderer draws the pixel right away
like pixelRGBA. Fully transparent pixels do not change the target and are skipped.
*/
static void _gfxBatchPixelAlpha(GFX_Batch *batch, Sint16 x, Sint16 y, Uint8 alpha)
{
	int i;
	float *xy;
	SDL_FColor color;
	SDL_FColor *colors;

	if (alpha == 0) {
		return;
	}
	if (batch->geometry < 0) {
		batch->geometry = _gfxBatchUseGeometry(batch);
	}
	if (!batch->geometry) {
		/* Same result as pixelRGBA: blending with alpha 255 replaces the pixel like SDL_BLENDMODE_NONE */
		if (batch->kind != GFX_BATCH_PIXEL) {
			_gfxBatchFlush(batch);
			batch->kind = GFX_BATCH_PIXEL;
			batch->result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(batch->renderer, SDL_BLENDMODE_BLEND));
			batch->colorSet = 0;
		}
		batch->result |= GFX_RESULT(SDL_SetRenderDrawColor(batch->renderer, batch->r, batch->g, batch->b, alpha));
		batch->result |= GFX_RESULT(SDL_RenderPoint(batch->renderer, x, y));
		return;
	}
	_gfxBatchPrepare(batch, GFX_BATCH_QUAD, GFX_BATCH_QUADS);
	xy = &batch->items.quads.xy[batch->count * 8];
	xy[0] = (float)x;
	xy[1] = (float)y;
	xy[2] = (float)x + 1.0f;
	xy[3] = (float)y;
	xy[4] = (float)x + 1.0f;
	xy[5] = (float)y + 1.0f;
	xy[6] = (float)x;
	xy[7] = (float)y + 1.0f;
	color.r = (float)batch->r / 255.0f;
	color.g = (float)batch->g / 255.0f;
	color.b = (float)batch->b / 255.0f;
	color.a = (float)alpha / 255.0f;
	colors = &batch->items.quads.colors[batch->count * 4];
	for (i = 0; i < 4; i++) {
		colors[i] = color;
	}
	batch->count++;
}

/*!
\brief Internal function to add an anti-aliased pixel with the alpha of the primitive scaled by weight (0..255) to a batch.
*/
static void _gfxBatchPixelWeight(GFX_Batch *batch, Sint16 x, Sint16 y, Uint32 weight)
{
	Uint32 ax = ((Uint32)batch->a * weight) >> 8;

	_gfxBatchPixelAlpha(batch, x, y, (ax > 255) ? 255 : (Uint8)ax);
}
/* ---- Rectangle */

/*!
\brief Draw rectangle with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the rectangle.
\param y1 Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param color The color value of the rectangle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int rectangleColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	return rectangleRGBA(renderer, x1, y1, x2, y2, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw rectangle with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the rectangle.
\param y1 Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param r The red value of the rectangle to draw. 
\param g The green value of the rectangle to draw. 
\param b The blue value of the rectangle to draw. 
\param a The alpha value of the rectangle to draw. 

\returns Returns 0 on success, -1 on failure.
*/
int rectangleRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result;
	Sint16 tmp;
	SDL_FRect rect;

	/*
	* Test for special cases of straight lines or single point 
	*/
	if (x1 == x2) {
		if (y1 == y2) {
			return (pixelRGBA(renderer, x1, y1, r, g, b, a));
		} else {
			return (vlineRGBA(renderer, x1, y1, y2, r, g, b, a));
		}
	} else {
		if (y1 == y2) {
			return (hlineRGBA(renderer, x1, x2, y1, r, g, b, a));
		}
	}

	/*
	* Swap x1, x2 if required 
	*/
	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	/*
	* Swap y1, y2 if required 
	*/
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	/* 
	* Create destination rect
	*/
	rect.x = (float)x1;
	rect.y = (float)y1;
	rect.w = (float)(x2 - x1);
	rect.h = (float)(y2 - y1);

	/*
	* Draw
	*/
	result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= GFX_RESULT(SDL_RenderRect(renderer, &rect));
	return result;
}

/* ---- Rounded Rectangle */

/*!
\brief Draw rounded-corner rectangle with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the rectangle.
\param y1 Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param rad The radius of the corner arc.
\param color The color value of the rectangle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int roundedRectangleColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color)
{
	return roundedRectangleRGBA(renderer, x1, y1, x2, y2, rad, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Internal function to plot the point (dx, dy), dx and dy > 0, in the four corners of a rounded rectangle.
*/
static void _roundedCorners(GFX_Batch *batch, Sint16 xx1, Sint16 yy1, Sint16 xx2, Sint16 yy2, Sint16 dx, Sint16 dy)
{
	_gfxBatchPoint(batch, (Sint16)(xx1 - dx), (Sint16)(yy1 - dy));
	_gfxBatchPoint(batch, (Sint16)(xx2 + dx), (Sint16)(yy1 - dy));
	_gfxBatchPoint(batch, (Sint16)(xx1 - dx), (Sint16)(yy2 + dy));
	_gfxBatchPoint(batch, (Sint16)(xx2 + dx), (Sint16)(yy2 + dy));
}
/*!
\brief Draw rounded-corner rectangle with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the rectangle.
\param y1 Y coordinate of the first point (i.e. top right) of the rectangle.
\param x2 X coordinate of the second point (i.e. bottom left) of the rectangle.
\param y2 Y coordinate of the second point (i.e. bottom left) of the rectangle.
\param rad The radius of the corner arc.
\param r The red value of the rectangle to draw. 
\param g The green value of the rectangle to draw. 
\param b The blue value of the rectangle to draw. 
\param a The alpha value of the rectangle to draw. 

\returns Returns 0 on success, -1 on failure.
*/
int roundedRectangleRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result = 0;
	Sint16 tmp;
	Sint16 w, h;
	Sint16 xx1, xx2;
	Sint16 yy1, yy2;
	Sint16 cx, cy, df, d_e, d_se;
	GFX_Batch batch;

	/*
	* Check renderer
	*/
	if (renderer == NULL) {
		return -1;
	}

	/*
	* Check radius vor valid range
	*/
	if (rad < 0) {
		return -1;
	}

	/*
	* Special case - no rounding
	*/
	if (rad <= 1) {
		return rectangleRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
	}

	/*
	* Test for special cases of straight lines or single point 
	*/
	if (x1 == x2) {
		if (y1 == y2) {
			return (pixelRGBA(renderer, x1, y1, r, g, b, a));
		} else {
			return (vlineRGBA(renderer, x1, y1, y2, r, g, b, a));
		}
	} else {
		if (y1 == y2) {
			return (hlineRGBA(renderer, x1, x2, y1, r, g, b, a));
		}
	}

	/*
	* Swap x1, x2 if required 
	*/
	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	/*
	* Swap y1, y2 if required 
	*/
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	/*
	* Calculate width&height 
	*/
	w = x2 - x1;
	h = y2 - y1;

	/*
	* Maybe adjust radius
	*/
	if ((rad * 2) > w) {
		rad = w / 2;
	}
	if ((rad * 2) > h) {
		rad = h / 2;
	}

	/*
	* Draw corners: quarter circles (midpoint algorithm) around the corner centers. Every pixel is
	* drawn once; the points on the axes belong to the straight edges (PRIM-016).
	*/
	xx1 = x1 + rad;
	xx2 = x2 - rad;
	yy1 = y1 + rad;
	yy2 = y2 - rad;
	_gfxBatchBegin(&batch, renderer, r, g, b, a);
	result |= batch.result;
	cx = 0;
	cy = rad;
	df = 1 - rad;
	d_e = 3;
	d_se = -2 * rad + 5;
	do {
		if (cx > 0) {
			_roundedCorners(&batch, xx1, yy1, xx2, yy2, cx, cy);
			if (cx != cy) {
				_roundedCorners(&batch, xx1, yy1, xx2, yy2, cy, cx);
			}
		}
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);

	/*
	* Draw lines
	*/
	_gfxBatchHLine(&batch, xx1, xx2, y1);
	_gfxBatchHLine(&batch, xx1, xx2, y2);
	_gfxBatchVLine(&batch, x1, yy1, yy2);
	_gfxBatchVLine(&batch, x2, yy1, yy2);

	return (result | _gfxBatchEnd(&batch));
}

/* ---- Rounded Box */

/*!
\brief Draw rounded-corner box (filled rectangle) with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the box.
\param y1 Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param rad The radius of the corner arcs of the box.
\param color The color value of the box to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int roundedBoxColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 rad, Uint32 color)
{
	return roundedBoxRGBA(renderer, x1, y1, x2, y2, rad, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw rounded-corner box (filled rectangle) with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the box.
\param y1 Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param rad The radius of the corner arcs of the box.
\param r The red value of the box to draw. 
\param g The green value of the box to draw. 
\param b The blue value of the box to draw. 
\param a The alpha value of the box to draw. 

\returns Returns 0 on success, -1 on failure.
*/
int roundedBoxRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2,
	Sint16 y2, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result;
	Sint16 w, h, r2, tmp;
	Sint16 cx, cy, ocx, ocy, df, d_e, d_se;
	Sint16 xpcx, xmcx, xpcy, xmcy;
	Sint16 ypcy, ymcy, ypcx, ymcx;
	Sint16 x, y, dx, dy;
	GFX_Batch batch;

	/* 
	* Check destination renderer 
	*/
	if (renderer == NULL) {
		return -1;
	}

	/*
	* Check radius vor valid range
	*/
	if (rad < 0) {
		return -1;
	}

	/*
	* Special case - no rounding
	*/
	if (rad <= 1) {
		return boxRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
	}

	/*
	* Test for special cases of straight lines or single point 
	*/
	if (x1 == x2) {
		if (y1 == y2) {
			return (pixelRGBA(renderer, x1, y1, r, g, b, a));
		} else {
			return (vlineRGBA(renderer, x1, y1, y2, r, g, b, a));
		}
	} else {
		if (y1 == y2) {
			return (hlineRGBA(renderer, x1, x2, y1, r, g, b, a));
		}
	}

	/*
	* Swap x1, x2 if required 
	*/
	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	/*
	* Swap y1, y2 if required 
	*/
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	/*
	* Calculate width&height 
	*/
	w = x2 - x1 + 1;
	h = y2 - y1 + 1;

	/*
	* Maybe adjust radius
	*/
	r2 = rad + rad;
	if (r2 > w) {
		rad = w / 2;
		r2 = rad + rad;
	}
	if (r2 > h) {
		rad = h / 2;
	}

	/* Setup filled circle drawing for corners (after clamping the radius, PRIM-004) */
	cx = 0;
	cy = rad;
	ocx = -1;
	ocy = -1;
	df = 1 - rad;
	d_e = 3;
	d_se = -2 * rad + 5;
	x = x1 + rad;
	y = y1 + rad;
	dx = x2 - x1 - rad - rad;
	dy = y2 - y1 - rad - rad;

	/*
	* Set color
	*/
	_gfxBatchBegin(&batch, renderer, r, g, b, a);

	/*
	* Draw corners
	*/
	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;
		if (ocy != cy) {
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				_gfxBatchHLine(&batch, xmcx, xpcx + dx, ypcy + dy);
				_gfxBatchHLine(&batch, xmcx, xpcx + dx, ymcy);
			} else {
				_gfxBatchHLine(&batch, xmcx, xpcx + dx, y);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					_gfxBatchHLine(&batch, xmcy, xpcy + dx, ymcx);
					_gfxBatchHLine(&batch, xmcy, xpcy + dx, ypcx + dy);
				} else {
					_gfxBatchHLine(&batch, xmcy, xpcy + dx, y);
				}
			}
			ocx = cx;
		}

		/*
		* Update 
		*/
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);

	/* Inside (full width; also when the corners touch horizontally, PRIM-012) */
	result = _gfxBatchEnd(&batch);
	if (dy > 0) {
		result |= boxRGBA(renderer, x1, y1 + rad + 1, x2, y2 - rad, r, g, b, a);
	}

	return (result);
}

/* ---- Box */

/*!
\brief Draw box (filled rectangle) with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the box.
\param y1 Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param color The color value of the box to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int boxColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	return boxRGBA(renderer, x1, y1, x2, y2, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw box (filled rectangle) with blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. top right) of the box.
\param y1 Y coordinate of the first point (i.e. top right) of the box.
\param x2 X coordinate of the second point (i.e. bottom left) of the box.
\param y2 Y coordinate of the second point (i.e. bottom left) of the box.
\param r The red value of the box to draw. 
\param g The green value of the box to draw. 
\param b The blue value of the box to draw. 
\param a The alpha value of the box to draw.

\returns Returns 0 on success, -1 on failure.
*/
int boxRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result;
	Sint16 tmp;
	SDL_FRect rect;

	/*
	* Test for special cases of straight lines or single point 
	*/
	if (x1 == x2) {
		if (y1 == y2) {
			return (pixelRGBA(renderer, x1, y1, r, g, b, a));
		} else {
			return (vlineRGBA(renderer, x1, y1, y2, r, g, b, a));
		}
	} else {
		if (y1 == y2) {
			return (hlineRGBA(renderer, x1, x2, y1, r, g, b, a));
		}
	}

	/*
	* Swap x1, x2 if required 
	*/
	if (x1 > x2) {
		tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	/*
	* Swap y1, y2 if required 
	*/
	if (y1 > y2) {
		tmp = y1;
		y1 = y2;
		y2 = tmp;
	}

	/* 
	* Create destination rect
	*/
	rect.x = (float)x1;
	rect.y = (float)y1;
	rect.w = (float)(x2 - x1 + 1);
	rect.h = (float)(y2 - y1 + 1);

	/*
	* Draw
	*/
	result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= GFX_RESULT(SDL_RenderFillRect(renderer, &rect));
	return result;
}

/* ----- Line */

/*!
\brief Draw line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the line.
\param y1 Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the seond point of the line.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int lineColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	return lineRGBA(renderer, x1, y1, x2, y2, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the line.
\param y1 Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param r The red value of the line to draw. 
\param g The green value of the line to draw. 
\param b The blue value of the line to draw. 
\param a The alpha value of the line to draw.

\returns Returns 0 on success, -1 on failure.
*/
int lineRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	/*
	* Draw
	*/
	int result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));
	result |= _gfxDrawLine(renderer, x1, y1, x2, y2);
	return result;
}

/* ---- AA Line */

#define AAlevels 256 /*!< Number of intensity levels of anti-aliased lines. */
#define AAbits   8 /*!< Bits of the intensity of anti-aliased lines. */

/*!
\brief Internal function to draw anti-aliased line with alpha blending and endpoint control.

This implementation of the Wu antialiasing code is based on Mike Abrash's
DDJ article which was reprinted as Chapter 42 of his Graphics Programming
Black Book, but has been optimized to work with SDL and utilizes 32-bit
fixed-point arithmetic by A. Schiffler. The endpoint control allows the
supression to draw the last pixel useful for rendering continous aa-lines
with alpha<255.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param r The red value of the aa-line to draw. 
\param g The green value of the aa-line to draw. 
\param b The blue value of the aa-line to draw. 
\param a The alpha value of the aa-line to draw.
\param draw_endpoint Flag indicating if the endpoint should be drawn; draw if non-zero.

\returns Returns 0 on success, -1 on failure.
*/
static void _aalineBatch(GFX_Batch *batch, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, int draw_endpoint)
{
	Sint32 xx0, yy0, xx1, yy1;
	int i;
	Uint32 intshift, erracc, erradj;
	Uint32 erracctmp, wgt;
	int dx, dy, tmp, xdir, y0p1, x0pxdir;

	/*
	* Keep on working with 32bit numbers 
	*/
	xx0 = x1;
	yy0 = y1;
	xx1 = x2;
	yy1 = y2;

	/*
	* Reorder points to make dy positive 
	*/
	if (yy0 > yy1) {
		tmp = yy0;
		yy0 = yy1;
		yy1 = tmp;
		tmp = xx0;
		xx0 = xx1;
		xx1 = tmp;
	}

	/*
	* Calculate distance 
	*/
	dx = xx1 - xx0;
	dy = yy1 - yy0;

	/*
	* Adjust for negative dx and set xdir 
	*/
	if (dx >= 0) {
		xdir = 1;
	} else {
		xdir = -1;
		dx = (-dx);
	}

	/*
	* Check for special cases 
	*/
	if (dx == 0) {
		/*
		* Vertical line 
		*/
		if (draw_endpoint) {
			_gfxBatchVLine(batch, x1, y1, y2);
		} else {
			if (dy > 0) {
				_gfxBatchVLine(batch, x1, (Sint16)yy0, (Sint16)(yy0 + dy));
			} else {
				_gfxBatchPoint(batch, x1, y1);
			}
		}
		return;
	} else if (dy == 0) {
		/*
		* Horizontal line 
		*/
		if (draw_endpoint) {
			_gfxBatchHLine(batch, x1, x2, y1);
		} else {
			if (dx > 0) {
				_gfxBatchHLine(batch, (Sint16)xx0, (Sint16)(xx0 + (xdir * dx)), y1);
			} else {
				_gfxBatchPoint(batch, x1, y1);
			}
		}
		return;
	} else if ((dx == dy) && (draw_endpoint)) {
		/*
		* Diagonal line (with endpoint)
		*/
		for (i = 0; i <= dx; i++) {
			_gfxBatchPoint(batch, (Sint16)(xx0 + xdir * i), (Sint16)(yy0 + i));
		}
		return;
	}


	/*
	* Line is not horizontal, vertical or diagonal (with endpoint)
	*/
	/*
	* Zero accumulator 
	*/
	erracc = 0;

	/*
	* # of bits by which to shift erracc to get intensity level 
	*/
	intshift = 32 - AAbits;

	/*
	* Draw the initial pixel in the foreground color 
	*/
	_gfxBatchPixelAlpha(batch, x1, y1, batch->a);

	/*
	* x-major or y-major? 
	*/
	if (dy > dx) {

		/*
		* y-major.  Calculate 16-bit fixed point fractional part of a pixel that
		* X advances every time Y advances 1 pixel, truncating the result so that
		* we won't overrun the endpoint along the X axis 
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dx << 32) / (Uint64)dy; 
		*/
		erradj = ((dx << 16) / dy) << 16;

		/*
		* draw all pixels other than the first and last 
		*/
		x0pxdir = xx0 + xdir;
		while (--dy) {
			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp) {
				/*
				* rollover in error accumulator, x coord advances 
				*/
				xx0 = x0pxdir;
				x0pxdir += xdir;
			}
			yy0++; /* y-major so always advance Y */

			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel. 
			*/
			wgt = (erracc >> intshift) & 255;
			_gfxBatchPixelWeight(batch, (Sint16)xx0, (Sint16)yy0, 255 - wgt);
			_gfxBatchPixelWeight(batch, (Sint16)x0pxdir, (Sint16)yy0, wgt);
		}

	} else {

		/*
		* x-major line.  Calculate 16-bit fixed-point fractional part of a pixel
		* that Y advances each time X advances 1 pixel, truncating the result so
		* that we won't overrun the endpoint along the X axis. 
		*/
		/*
		* Not-so-portable version: erradj = ((Uint64)dy << 32) / (Uint64)dx; 
		*/
		erradj = ((dy << 16) / dx) << 16;

		/*
		* draw all pixels other than the first and last 
		*/
		y0p1 = yy0 + 1;
		while (--dx) {

			erracctmp = erracc;
			erracc += erradj;
			if (erracc <= erracctmp) {
				/*
				* Accumulator turned over, advance y 
				*/
				yy0 = y0p1;
				y0p1++;
			}
			xx0 += xdir; /* x-major so always advance X */
			/*
			* the AAbits most significant bits of erracc give us the intensity
			* weighting for this pixel, and the complement of the weighting for
			* the paired pixel. 
			*/
			wgt = (erracc >> intshift) & 255;
			_gfxBatchPixelWeight(batch, (Sint16)xx0, (Sint16)yy0, 255 - wgt);
			_gfxBatchPixelWeight(batch, (Sint16)xx0, (Sint16)y0p1, wgt);
		}
	}

	/*
	* Do we have to draw the endpoint 
	*/
	if (draw_endpoint) {
		/*
		* Draw final pixel, always exactly intersected by the line and doesn't
		* need to be weighted. 
		*/
		_gfxBatchPixelAlpha(batch, x2, y2, batch->a);
	}
}

/*!
\brief Internal function to draw anti-aliased line with alpha blending and endpoint control.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param r The red value of the aa-line to draw. 
\param g The green value of the aa-line to draw. 
\param b The blue value of the aa-line to draw. 
\param a The alpha value of the aa-line to draw.
\param draw_endpoint Flag indicating if the endpoint should be drawn; draw if non-zero.

\returns Returns 0 on success, -1 on failure.
*/
static int _aalineRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int draw_endpoint)
{
	GFX_Batch batch;

	_gfxBatchBegin(&batch, renderer, r, g, b, a);
	_aalineBatch(&batch, x1, y1, x2, y2, draw_endpoint);
	return (_gfxBatchEnd(&batch));
}

/*!
\brief Draw anti-aliased line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param color The color value of the aa-line to draw (0xRRGGBBAA).

\returns Returns 0 on success, -1 on failure.
*/
int aalineColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint32 color)
{
	return _aalineRGBA(renderer, x1, y1, x2, y2, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), 1);
}

/*!
\brief Draw anti-aliased line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-line.
\param y1 Y coordinate of the first point of the aa-line.
\param x2 X coordinate of the second point of the aa-line.
\param y2 Y coordinate of the second point of the aa-line.
\param r The red value of the aa-line to draw. 
\param g The green value of the aa-line to draw. 
\param b The blue value of the aa-line to draw. 
\param a The alpha value of the aa-line to draw.

\returns Returns 0 on success, -1 on failure.
*/
int aalineRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _aalineRGBA(renderer, x1, y1, x2, y2, r, g, b, a, 1);
}

/* ----- Circle */

/*!
\brief Draw circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the circle.
\param y Y coordinate of the center of the circle.
\param rad Radius in pixels of the circle.
\param color The color value of the circle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int circleColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
	return ellipseRGBA(renderer, x, y, rad, rad, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the circle.
\param y Y coordinate of the center of the circle.
\param rad Radius in pixels of the circle.
\param r The red value of the circle to draw. 
\param g The green value of the circle to draw. 
\param b The blue value of the circle to draw. 
\param a The alpha value of the circle to draw.

\returns Returns 0 on success, -1 on failure.
*/
int circleRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return ellipseRGBA(renderer, x, y, rad, rad, r, g, b, a);
}

/* ----- Arc */

/*!
\brief Arc with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the arc.
\param y Y coordinate of the center of the arc.
\param rad Radius in pixels of the arc.
\param start Starting radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param end Ending radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param color The color value of the arc to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int arcColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color)
{
	return arcRGBA(renderer, x, y, rad, start, end, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Arc with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the arc.
\param y Y coordinate of the center of the arc.
\param rad Radius in pixels of the arc.
\param start Starting radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param end Ending radius in degrees of the arc. 0 degrees is down, increasing counterclockwise.
\param r The red value of the arc to draw. 
\param g The green value of the arc to draw. 
\param b The blue value of the arc to draw. 
\param a The alpha value of the arc to draw.

\returns Returns 0 on success, -1 on failure.
*/
/* TODO: rewrite algorithm; arc endpoints are not always drawn */
int arcRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	GFX_Batch batch;
	Sint16 cx = 0;
	Sint16 cy = rad;
	Sint16 df = 1 - rad;
	Sint16 d_e = 3;
	Sint16 d_se = -2 * rad + 5;
	Sint16 xpcx, xmcx, xpcy, xmcy;
	Sint16 ypcy, ymcy, ypcx, ymcx;
	Uint8 drawoct;
	int startoct, endoct, oct, stopval_start = 0, stopval_end = 0;
	double dstart, dend, temp = 0.;

	/*
	* Sanity check radius 
	*/
	if (rad < 0) {
		return (-1);
	}

	/*
	* Special case for rad=0 - draw a point 
	*/
	if (rad == 0) {
		return (pixelRGBA(renderer, x, y, r, g, b, a));
	}

	/*
	* Special case for a full turn (e.g. 0..360) - draw a circle (PRIM-005)
	*/
	if ((start != end) && ((((int)end - (int)start) % 360) == 0)) {
		return (circleRGBA(renderer, x, y, rad, r, g, b, a));
	}

	/*
	 Octant labeling
	      
	  \ 5 | 6 /
	   \  |  /
	  4 \ | / 7
	     \|/
	------+------ +x
	     /|\
	  3 / | \ 0
	   /  |  \
	  / 2 | 1 \
	      +y

	 Initially reset bitmask to 0x00000000
	 the set whether or not to keep drawing a given octant.
	 For example: 0x00111100 means we're drawing in octants 2-5
	*/
	drawoct = 0;

	/*
	* Fixup angles
	*/
	/* 0 <= start & end < 360; note that sometimes start > end - if so, arc goes back through 0. */
	start = (Sint16)(((start % 360) + 360) % 360);
	end = (Sint16)(((end % 360) + 360) % 360);

	/* now, we find which octants we're drawing in. */
	startoct = start / 45;
	endoct = end / 45;
	oct = startoct - 1;

	/* stopval_start, stopval_end; what values of cx to stop at. */
	do {
		oct = (oct + 1) & 7; /* 0..7 (start/end are normalized to 0..359) */

		if (oct == startoct) {
			/* need to compute stopval_start for this octant.  Look at picture above if this is unclear */
			dstart = (double)start;
			switch (oct) {
				case 0:
				case 3:
					temp = sin(dstart * M_PI / 180.);
					break;
				case 1:
				case 6:
					temp = cos(dstart * M_PI / 180.);
					break;
				case 2:
				case 5:
					temp = -cos(dstart * M_PI / 180.);
					break;
				case 4:
				case 7:
				default:
					temp = -sin(dstart * M_PI / 180.);
					break;
			}
			temp *= rad;
			stopval_start = (int)temp;

			/* 
			This isn't arbitrary, but requires graph paper to explain well.
			The basic idea is that we're always changing drawoct after we draw, so we
			stop immediately after we render the last sensible pixel at x = ((int)temp).
			and whether to draw in this octant initially
			*/
			if (oct % 2)
				drawoct |= (1 << oct); /* this is basically like saying drawoct[oct] = true, if drawoct were a bool array */
			else
				drawoct &= 255 - (1 << oct); /* this is basically like saying drawoct[oct] = false */
		}
		if (oct == endoct) {
			/* need to compute stopval_end for this octant */
			dend = (double)end;
			switch (oct) {
				case 0:
				case 3:
					temp = sin(dend * M_PI / 180);
					break;
				case 1:
				case 6:
					temp = cos(dend * M_PI / 180);
					break;
				case 2:
				case 5:
					temp = -cos(dend * M_PI / 180);
					break;
				case 4:
				case 7:
				default:
					temp = -sin(dend * M_PI / 180);
					break;
			}
			temp *= rad;
			stopval_end = (int)temp;

			/* and whether to draw in this octant initially */
			if (startoct == endoct) {
				/* note:      we start drawing, stop, then start again in this case */
				/* otherwise: we only draw in this octant, so initialize it to false, it will get set back to true */
				if (start > end) {
					/* unfortunately, if we're in the same octant and need to draw over the whole circle, */
					/* we need to set the rest to true, because the while loop will end at the bottom. */
					drawoct = 255;
				} else {
					drawoct &= 255 - (1 << oct);
				}
			} else if (oct % 2)
				drawoct &= 255 - (1 << oct);
			else
				drawoct |= (1 << oct);
		} else if (oct != startoct) { /* already verified that it's != endoct */
			drawoct |= (1 << oct); /* draw this entire segment */
		}
	} while (oct != endoct);

	/* so now we have what octants to draw and when to draw them. all that's left is the actual raster code. */

	/*
	* Set color 
	*/
	_gfxBatchBegin(&batch, renderer, r, g, b, a);

	/*
	* Draw arc 
	*/
	do {
		ypcy = y + cy;
		ymcy = y - cy;
		if (cx > 0) {
			xpcx = x + cx;
			xmcx = x - cx;

			/* always check if we're drawing a certain octant before adding a pixel to that octant. */
			if (drawoct & 4) _gfxBatchPoint(&batch, xmcx, ypcy);
			if (drawoct & 2) _gfxBatchPoint(&batch, xpcx, ypcy);
			if (drawoct & 32) _gfxBatchPoint(&batch, xmcx, ymcy);
			if (drawoct & 64) _gfxBatchPoint(&batch, xpcx, ymcy);
		} else {
			if (drawoct & 96) _gfxBatchPoint(&batch, x, ymcy);
			if (drawoct & 6) _gfxBatchPoint(&batch, x, ypcy);
		}

		xpcy = x + cy;
		xmcy = x - cy;
		if (cx > 0 && cx != cy) {
			ypcx = y + cx;
			ymcx = y - cx;
			if (drawoct & 8) _gfxBatchPoint(&batch, xmcy, ypcx);
			if (drawoct & 1) _gfxBatchPoint(&batch, xpcy, ypcx);
			if (drawoct & 16) _gfxBatchPoint(&batch, xmcy, ymcx);
			if (drawoct & 128) _gfxBatchPoint(&batch, xpcy, ymcx);
		} else if (cx == 0) {
			if (drawoct & 24) _gfxBatchPoint(&batch, xmcy, y);
			if (drawoct & 129) _gfxBatchPoint(&batch, xpcy, y);
		}

		/*
		* Update whether we're drawing an octant
		*/
		if (stopval_start == cx) {
			/* works like an on-off switch. */
			/* This is just in case start & end are in the same octant. */
			if (drawoct & (1 << startoct))
				drawoct &= 255 - (1 << startoct);
			else
				drawoct |= (1 << startoct);
		}
		if (stopval_end == cx) {
			if (drawoct & (1 << endoct))
				drawoct &= 255 - (1 << endoct);
			else
				drawoct |= (1 << endoct);
		}

		/*
		* Update pixels
		*/
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);

	return (_gfxBatchEnd(&batch));
}

/* ----- AA Circle */

/*!
\brief Draw anti-aliased circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the aa-circle.
\param y Y coordinate of the center of the aa-circle.
\param rad Radius in pixels of the aa-circle.
\param color The color value of the aa-circle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int aacircleColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
	return aaellipseRGBA(renderer, x, y, rad, rad, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw anti-aliased circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the aa-circle.
\param y Y coordinate of the center of the aa-circle.
\param rad Radius in pixels of the aa-circle.
\param r The red value of the aa-circle to draw. 
\param g The green value of the aa-circle to draw. 
\param b The blue value of the aa-circle to draw. 
\param a The alpha value of the aa-circle to draw.

\returns Returns 0 on success, -1 on failure.
*/
int aacircleRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	/*
	* Draw 
	*/
	return aaellipseRGBA(renderer, x, y, rad, rad, r, g, b, a);
}

/* ----- Ellipse */

/*!
\brief Internal function to draw pixels or lines in 4 quadrants.

\param batch The batch to draw with.
\param x X coordinate of the center of the quadrant.
\param y Y coordinate of the center of the quadrant.
\param dx X offset in pixels of the corners of the quadrant.
\param dy Y offset in pixels of the corners of the quadrant.
\param f Flag indicating if the quadrant should be filled (1) or not (0).
*/
static void _drawQuadrants(GFX_Batch *batch, Sint16 x, Sint16 y, Sint16 dx, Sint16 dy, Sint32 f)
{
	Sint16 xpdx, xmdx;
	Sint16 ypdy, ymdy;

	if (dx == 0) {
		if (dy == 0) {
			_gfxBatchPoint(batch, x, y);
		} else {
			ypdy = y + dy;
			ymdy = y - dy;
			if (f) {
				_gfxBatchVLine(batch, x, ymdy, ypdy);
			} else {
				_gfxBatchPoint(batch, x, ypdy);
				_gfxBatchPoint(batch, x, ymdy);
			}
		}
	} else {
		xpdx = x + dx;
		xmdx = x - dx;
		ypdy = y + dy;
		ymdy = y - dy;
		if (f) {
			_gfxBatchVLine(batch, xpdx, ymdy, ypdy);
			_gfxBatchVLine(batch, xmdx, ymdy, ypdy);
		} else {
			_gfxBatchPoint(batch, xpdx, ypdy);
			_gfxBatchPoint(batch, xmdx, ypdy);
			if (dy != 0) { /* the points on the horizontal axis only once (PRIM-016) */
				_gfxBatchPoint(batch, xpdx, ymdy);
				_gfxBatchPoint(batch, xmdx, ymdy);
			}
		}
	}
}
/*!
\brief Internal function to draw ellipse or filled ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the ellipse.
\param y Y coordinate of the center of the ellipse.
\param rx Horizontal radius in pixels of the ellipse.
\param ry Vertical radius in pixels of the ellipse.
\param r The red value of the ellipse to draw. 
\param g The green value of the ellipse to draw. 
\param b The blue value of the ellipse to draw. 
\param a The alpha value of the ellipse to draw.
\param f Flag indicating if the ellipse should be filled (1) or not (0).

\returns Returns 0 on success, -1 on failure.
*/
#define DEFAULT_ELLIPSE_OVERSCAN 4
static int _ellipseRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a, Sint32 f)
{
	Sint32 rxi, ryi;
	Sint32 rx2, ry2, rx22, ry22;
	Sint32 error;
	Sint32 curX, curY, curXp1, curYm1;
	Sint32 scrX, scrY, oldX, oldY;
	Sint32 deltaX, deltaY;
	Sint32 ellipseOverscan;
	GFX_Batch batch;

	/*
	* Sanity check radii 
	*/
	if ((rx < 0) || (ry < 0)) {
		return (-1);
	}

	/*
	* Set color
	*/
	_gfxBatchBegin(&batch, renderer, r, g, b, a);

	/*
	* Special cases for rx=0 and/or ry=0: draw a hline/vline/pixel 
	*/
	if (rx == 0) {
		if (ry == 0) {
			_gfxBatchPoint(&batch, x, y);
		} else {
			_gfxBatchVLine(&batch, x, y - ry, y + ry);
		}
		return (_gfxBatchEnd(&batch));
	} else {
		if (ry == 0) {
			_gfxBatchHLine(&batch, x - rx, x + rx, y);
			return (_gfxBatchEnd(&batch));
		}
	}

	/*
 	 * Adjust overscan 
	 */
	rxi = rx;
	ryi = ry;
	if (rxi >= 512 || ryi >= 512) {
		ellipseOverscan = DEFAULT_ELLIPSE_OVERSCAN / 4;
	} else if (rxi >= 256 || ryi >= 256) {
		ellipseOverscan = DEFAULT_ELLIPSE_OVERSCAN / 2;
	} else {
		ellipseOverscan = DEFAULT_ELLIPSE_OVERSCAN / 1;
	}

	/*
	 * Top/bottom center points.
	 */
	oldX = scrX = 0;
	oldY = ryi;
	_drawQuadrants(&batch, x, y, 0, ry, f);

	/* Midpoint ellipse algorithm with overdraw */
	rxi *= ellipseOverscan;
	ryi *= ellipseOverscan;
	rx2 = rxi * rxi;
	rx22 = rx2 + rx2;
	ry2 = ryi * ryi;
	ry22 = ry2 + ry2;
	curX = 0;
	curY = ryi;
	deltaX = 0;
	deltaY = rx22 * curY;

	/* Points in segment 1 */
	error = ry2 - rx2 * ryi + rx2 / 4;
	while (deltaX <= deltaY) {
		curX++;
		deltaX += ry22;

		error += deltaX + ry2;
		if (error >= 0) {
			curY--;
			deltaY -= rx22;
			error -= deltaY;
		}

		scrX = curX / ellipseOverscan;
		scrY = curY / ellipseOverscan;
		if ((scrX != oldX && scrY == oldY) || (scrX != oldX && scrY != oldY)) {
			_drawQuadrants(&batch, x, y, (Sint16)scrX, (Sint16)scrY, f);
			oldX = scrX;
			oldY = scrY;
		}
	}

	/* Points in segment 2 */
	if (curY > 0) {
		curXp1 = curX + 1;
		curYm1 = curY - 1;
		error = ry2 * curX * curXp1 + ((ry2 + 3) / 4) + rx2 * curYm1 * curYm1 - rx2 * ry2;
		while (curY > 0) {
			curY--;
			deltaY -= rx22;

			error += rx2;
			error -= deltaY;

			if (error <= 0) {
				curX++;
				deltaX += ry22;
				error += deltaX;
			}

			scrX = curX / ellipseOverscan;
			scrY = curY / ellipseOverscan;
			if ((scrX != oldX && scrY == oldY) || (scrX != oldX && scrY != oldY)) {
				oldY--;
				for (; oldY >= scrY; oldY--) {
					_drawQuadrants(&batch, x, y, (Sint16)scrX, (Sint16)oldY, f);
					/* prevent overdraw */
					if (f) {
						oldY = scrY - 1;
					}
				}
				oldX = scrX;
				oldY = scrY;
			}
		}

		/* Remaining points in vertical */
		if (!f) {
			oldY--;
			for (; oldY >= 0; oldY--) {
				_drawQuadrants(&batch, x, y, (Sint16)scrX, (Sint16)oldY, f);
			}
		}
	}

	return (_gfxBatchEnd(&batch));
}

/*!
\brief Draw ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the ellipse.
\param y Y coordinate of the center of the ellipse.
\param rx Horizontal radius in pixels of the ellipse.
\param ry Vertical radius in pixels of the ellipse.
\param color The color value of the ellipse to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int ellipseColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
	return _ellipseRGBA(renderer, x, y, rx, ry, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), 0);
}

/*!
\brief Draw ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the ellipse.
\param y Y coordinate of the center of the ellipse.
\param rx Horizontal radius in pixels of the ellipse.
\param ry Vertical radius in pixels of the ellipse.
\param r The red value of the ellipse to draw. 
\param g The green value of the ellipse to draw. 
\param b The blue value of the ellipse to draw. 
\param a The alpha value of the ellipse to draw.

\returns Returns 0 on success, -1 on failure.
*/
int ellipseRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _ellipseRGBA(renderer, x, y, rx, ry, r, g, b, a, 0);
}

/* ----- Filled Circle */

/*!
\brief Draw filled circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled circle.
\param y Y coordinate of the center of the filled circle.
\param rad Radius in pixels of the filled circle.
\param color The color value of the filled circle to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int filledCircleColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint32 color)
{
	return filledEllipseRGBA(renderer, x, y, rad, rad, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw filled circle with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled circle.
\param y Y coordinate of the center of the filled circle.
\param rad Radius in pixels of the filled circle.
\param r The red value of the filled circle to draw. 
\param g The green value of the filled circle to draw. 
\param b The blue value of the filled circle to draw. 
\param a The alpha value of the filled circle to draw.

\returns Returns 0 on success, -1 on failure.
*/
int filledCircleRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _ellipseRGBA(renderer, x, y, rad, rad, r, g, b, a, 1);
}


/* ----- AA Ellipse */

/*!
\brief Draw anti-aliased ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the aa-ellipse.
\param y Y coordinate of the center of the aa-ellipse.
\param rx Horizontal radius in pixels of the aa-ellipse.
\param ry Vertical radius in pixels of the aa-ellipse.
\param color The color value of the aa-ellipse to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int aaellipseColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
	return aaellipseRGBA(renderer, x, y, rx, ry, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw anti-aliased ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the aa-ellipse.
\param y Y coordinate of the center of the aa-ellipse.
\param rx Horizontal radius in pixels of the aa-ellipse.
\param ry Vertical radius in pixels of the aa-ellipse.
\param r The red value of the aa-ellipse to draw. 
\param g The green value of the aa-ellipse to draw. 
\param b The blue value of the aa-ellipse to draw. 
\param a The alpha value of the aa-ellipse to draw.

\returns Returns 0 on success, -1 on failure.
*/
int aaellipseRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int i;
	GFX_Batch batch;
	int a2, b2, ds, dt, dxt, t, s, d;
	Sint16 xp, yp, xs, ys, dyt, od, xx, yy, xc2, yc2;
	float cp;
	double sab;
	Uint8 weight, iweight;

	/*
	* Sanity check radii 
	*/
	if ((rx < 0) || (ry < 0)) {
		return (-1);
	}

	/*
	* Special cases for rx=0 and/or ry=0: draw a hline/vline/pixel 
	*/
	if (rx == 0) {
		if (ry == 0) {
			return (pixelRGBA(renderer, x, y, r, g, b, a));
		} else {
			return (vlineRGBA(renderer, x, y - ry, y + ry, r, g, b, a));
		}
	} else {
		if (ry == 0) {
			return (hlineRGBA(renderer, x - rx, x + rx, y, r, g, b, a));
		}
	}

	/* Variable setup */
	a2 = rx * rx;
	b2 = ry * ry;

	ds = 2 * a2;
	dt = 2 * b2;

	xc2 = 2 * x;
	yc2 = 2 * y;

	sab = sqrt((double)(a2 + b2));
	od = (Sint16)lrint(sab * 0.01) + 1; /* introduce some overdraw */
	dxt = (Sint16)lrint((double)a2 / sab) + od;

	t = 0;
	s = -2 * a2 * ry;
	d = 0;

	xp = x;
	yp = y - ry;

	/* Draw */
	_gfxBatchBegin(&batch, renderer, r, g, b, a);

	/* "End points" */
	_gfxBatchPixelAlpha(&batch, xp, yp, a);
	_gfxBatchPixelAlpha(&batch, xc2 - xp, yp, a);
	_gfxBatchPixelAlpha(&batch, xp, yc2 - yp, a);
	_gfxBatchPixelAlpha(&batch, xc2 - xp, yc2 - yp, a);

	for (i = 1; i <= dxt; i++) {
		xp--;
		d += t - b2;

		if (d >= 0)
			ys = yp - 1;
		else if ((d - s - a2) > 0) {
			if ((2 * d - s - a2) >= 0)
				ys = yp + 1;
			else {
				ys = yp;
				yp++;
				d -= s + a2;
				s += ds;
			}
		} else {
			yp++;
			ys = yp + 1;
			d -= s + a2;
			s += ds;
		}

		t -= dt;

		/* Calculate alpha */
		if (s != 0) {
			cp = (float)abs(d) / (float)abs(s);
			if (cp > 1.0) {
				cp = 1.0;
			}
		} else {
			cp = 1.0;
		}

		/* Calculate weights */
		weight = (Uint8)(cp * 255);
		iweight = 255 - weight;

		/* Upper half */
		xx = xc2 - xp;
		_gfxBatchPixelWeight(&batch, xp, yp, iweight);
		_gfxBatchPixelWeight(&batch, xx, yp, iweight);

		_gfxBatchPixelWeight(&batch, xp, ys, weight);
		_gfxBatchPixelWeight(&batch, xx, ys, weight);

		/* Lower half */
		yy = yc2 - yp;
		_gfxBatchPixelWeight(&batch, xp, yy, iweight);
		_gfxBatchPixelWeight(&batch, xx, yy, iweight);

		yy = yc2 - ys;
		_gfxBatchPixelWeight(&batch, xp, yy, weight);
		_gfxBatchPixelWeight(&batch, xx, yy, weight);
	}

	/* Replaces original approximation code dyt = abs(yp - yc); */
	dyt = (Sint16)lrint((double)b2 / sab) + od;

	for (i = 1; i <= dyt; i++) {
		yp++;
		d -= s + a2;

		if (d <= 0)
			xs = xp + 1;
		else if ((d + t - b2) < 0) {
			if ((2 * d + t - b2) <= 0)
				xs = xp - 1;
			else {
				xs = xp;
				xp--;
				d += t - b2;
				t -= dt;
			}
		} else {
			xp--;
			xs = xp - 1;
			d += t - b2;
			t -= dt;
		}

		s += ds;

		/* Calculate alpha */
		if (t != 0) {
			cp = (float)abs(d) / (float)abs(t);
			if (cp > 1.0) {
				cp = 1.0;
			}
		} else {
			cp = 1.0;
		}

		/* Calculate weight */
		weight = (Uint8)(cp * 255);
		iweight = 255 - weight;

		/* Left half */
		xx = xc2 - xp;
		yy = yc2 - yp;
		_gfxBatchPixelWeight(&batch, xp, yp, iweight);
		_gfxBatchPixelWeight(&batch, xx, yp, iweight);

		_gfxBatchPixelWeight(&batch, xp, yy, iweight);
		_gfxBatchPixelWeight(&batch, xx, yy, iweight);

		/* Right half */
		xx = xc2 - xs;
		_gfxBatchPixelWeight(&batch, xs, yp, weight);
		_gfxBatchPixelWeight(&batch, xx, yp, weight);

		_gfxBatchPixelWeight(&batch, xs, yy, weight);
		_gfxBatchPixelWeight(&batch, xx, yy, weight);
	}

	return (_gfxBatchEnd(&batch));
}

/* ---- Filled Ellipse */

/*!
\brief Draw filled ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled ellipse.
\param y Y coordinate of the center of the filled ellipse.
\param rx Horizontal radius in pixels of the filled ellipse.
\param ry Vertical radius in pixels of the filled ellipse.
\param color The color value of the filled ellipse to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int filledEllipseColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint32 color)
{
	return _ellipseRGBA(renderer, x, y, rx, ry, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), 1);
}

/*!
\brief Draw filled ellipse with blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled ellipse.
\param y Y coordinate of the center of the filled ellipse.
\param rx Horizontal radius in pixels of the filled ellipse.
\param ry Vertical radius in pixels of the filled ellipse.
\param r The red value of the filled ellipse to draw. 
\param g The green value of the filled ellipse to draw. 
\param b The blue value of the filled ellipse to draw. 
\param a The alpha value of the filled ellipse to draw.

\returns Returns 0 on success, -1 on failure.
*/
int filledEllipseRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rx, Sint16 ry, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _ellipseRGBA(renderer, x, y, rx, ry, r, g, b, a, 1);
}

/* ----- Pie */

/*!
\brief Internal float (low-speed) pie-calc implementation by drawing polygons.

Note: Determines vertex array and uses polygon or filledPolygon drawing routines to render.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param r The red value of the pie to draw. 
\param g The green value of the pie to draw. 
\param b The blue value of the pie to draw. 
\param a The alpha value of the pie to draw.
\param filled Flag indicating if the pie should be filled (=1) or not (=0).

\returns Returns 0 on success, -1 on failure.
*/
/* TODO: rewrite algorithm; pie is not always accurate */
static int _pieRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a, Uint8 filled)
{
	int result;
	double angle, start_angle, end_angle;
	double deltaAngle;
	double dr;
	int numpoints, i;
	Sint16 *vx, *vy;

	/*
	* Sanity check radii 
	*/
	if (rad < 0) {
		return (-1);
	}

	/*
	* Fixup angles
	*/
	start = start % 360;
	end = end % 360;

	/*
	* Special case for rad=0 - draw a point 
	*/
	if (rad == 0) {
		return (pixelRGBA(renderer, x, y, r, g, b, a));
	}

	/*
	* Variable setup 
	*/
	dr = (double)rad;
	deltaAngle = 3.0 / dr;
	start_angle = (double)start * (2.0 * M_PI / 360.0);
	end_angle = (double)end * (2.0 * M_PI / 360.0);
	if (start > end) {
		end_angle += (2.0 * M_PI);
	}

	/* We will always have at least 2 points */
	numpoints = 2;

	/* Count points (rather than calculating it) */
	angle = start_angle;
	while (angle < end_angle) {
		angle += deltaAngle;
		numpoints++;
	}

	/* Allocate combined vertex array */
	vx = (Sint16 *)SDL_malloc(2 * sizeof(Sint16) * (size_t)numpoints);
	if (vx == NULL) {
		return (-1);
	}

	/* Second half holds the y coordinates */
	vy = vx + numpoints;

	/* Center */
	vx[0] = x;
	vy[0] = y;

	/* First vertex */
	angle = start_angle;
	vx[1] = (Sint16)(x + (int)(dr * cos(angle)));
	vy[1] = (Sint16)(y + (int)(dr * sin(angle)));

	if (numpoints < 3) {
		result = lineRGBA(renderer, vx[0], vy[0], vx[1], vy[1], r, g, b, a);
	} else {
		/* Calculate other vertices */
		i = 2;
		angle = start_angle;
		/* Same steps as the counting loop above; i < numpoints is implied (SA-001) */
		while ((angle < end_angle) && (i < numpoints)) {
			angle += deltaAngle;
			if (angle > end_angle) {
				angle = end_angle;
			}
			vx[i] = (Sint16)(x + (int)(dr * cos(angle)));
			vy[i] = (Sint16)(y + (int)(dr * sin(angle)));
			i++;
		}

		/* Draw */
		if (filled) {
			result = filledPolygonRGBA(renderer, vx, vy, numpoints, r, g, b, a);
		} else {
			result = polygonRGBA(renderer, vx, vy, numpoints, r, g, b, a);
		}
	}

	/* Free combined vertex array */
	SDL_free(vx);

	return (result);
}

/*!
\brief Draw pie (outline) with alpha blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param color The color value of the pie to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int pieColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad,
	Sint16 start, Sint16 end, Uint32 color)
{
	return _pieRGBA(renderer, x, y, rad, start, end, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), 0);
}

/*!
\brief Draw pie (outline) with alpha blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the pie.
\param y Y coordinate of the center of the pie.
\param rad Radius in pixels of the pie.
\param start Starting radius in degrees of the pie.
\param end Ending radius in degrees of the pie.
\param r The red value of the pie to draw. 
\param g The green value of the pie to draw. 
\param b The blue value of the pie to draw. 
\param a The alpha value of the pie to draw.

\returns Returns 0 on success, -1 on failure.
*/
int pieRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad,
	Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _pieRGBA(renderer, x, y, rad, start, end, r, g, b, a, 0);
}

/*!
\brief Draw filled pie with alpha blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled pie.
\param y Y coordinate of the center of the filled pie.
\param rad Radius in pixels of the filled pie.
\param start Starting radius in degrees of the filled pie.
\param end Ending radius in degrees of the filled pie.
\param color The color value of the filled pie to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int filledPieColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad, Sint16 start, Sint16 end, Uint32 color)
{
	return _pieRGBA(renderer, x, y, rad, start, end, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), 1);
}

/*!
\brief Draw filled pie with alpha blending.

\param renderer The renderer to draw on.
\param x X coordinate of the center of the filled pie.
\param y Y coordinate of the center of the filled pie.
\param rad Radius in pixels of the filled pie.
\param start Starting radius in degrees of the filled pie.
\param end Ending radius in degrees of the filled pie.
\param r The red value of the filled pie to draw. 
\param g The green value of the filled pie to draw. 
\param b The blue value of the filled pie to draw. 
\param a The alpha value of the filled pie to draw.

\returns Returns 0 on success, -1 on failure.
*/
int filledPieRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 rad,
	Sint16 start, Sint16 end, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return _pieRGBA(renderer, x, y, rad, start, end, r, g, b, a, 1);
}

/* ------ Trigon */

/*!
\brief Draw trigon (triangle outline) with alpha blending.

Note: Creates vertex array and uses polygon routine to render.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the trigon.
\param y1 Y coordinate of the first point of the trigon.
\param x2 X coordinate of the second point of the trigon.
\param y2 Y coordinate of the second point of the trigon.
\param x3 X coordinate of the third point of the trigon.
\param y3 Y coordinate of the third point of the trigon.
\param color The color value of the trigon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int trigonColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (polygonColor(renderer, vx, vy, 3, color));
}

/*!
\brief Draw trigon (triangle outline) with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the trigon.
\param y1 Y coordinate of the first point of the trigon.
\param x2 X coordinate of the second point of the trigon.
\param y2 Y coordinate of the second point of the trigon.
\param x3 X coordinate of the third point of the trigon.
\param y3 Y coordinate of the third point of the trigon.
\param r The red value of the trigon to draw. 
\param g The green value of the trigon to draw. 
\param b The blue value of the trigon to draw. 
\param a The alpha value of the trigon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int trigonRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (polygonRGBA(renderer, vx, vy, 3, r, g, b, a));
}

/* ------ AA-Trigon */

/*!
\brief Draw anti-aliased trigon (triangle outline) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-trigon.
\param y1 Y coordinate of the first point of the aa-trigon.
\param x2 X coordinate of the second point of the aa-trigon.
\param y2 Y coordinate of the second point of the aa-trigon.
\param x3 X coordinate of the third point of the aa-trigon.
\param y3 Y coordinate of the third point of the aa-trigon.
\param color The color value of the aa-trigon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int aatrigonColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (aapolygonColor(renderer, vx, vy, 3, color));
}

/*!
\brief Draw anti-aliased trigon (triangle outline) with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the aa-trigon.
\param y1 Y coordinate of the first point of the aa-trigon.
\param x2 X coordinate of the second point of the aa-trigon.
\param y2 Y coordinate of the second point of the aa-trigon.
\param x3 X coordinate of the third point of the aa-trigon.
\param y3 Y coordinate of the third point of the aa-trigon.
\param r The red value of the aa-trigon to draw. 
\param g The green value of the aa-trigon to draw. 
\param b The blue value of the aa-trigon to draw. 
\param a The alpha value of the aa-trigon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int aatrigonRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (aapolygonRGBA(renderer, vx, vy, 3, r, g, b, a));
}

/* ------ Filled Trigon */

/*!
\brief Draw filled trigon (triangle) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the filled trigon.
\param y1 Y coordinate of the first point of the filled trigon.
\param x2 X coordinate of the second point of the filled trigon.
\param y2 Y coordinate of the second point of the filled trigon.
\param x3 X coordinate of the third point of the filled trigon.
\param y3 Y coordinate of the third point of the filled trigon.
\param color The color value of the filled trigon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int filledTrigonColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3, Uint32 color)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (filledPolygonColor(renderer, vx, vy, 3, color));
}

/*!
\brief Draw filled trigon (triangle) with alpha blending.

Note: Creates vertex array and uses aapolygon routine to render.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the filled trigon.
\param y1 Y coordinate of the first point of the filled trigon.
\param x2 X coordinate of the second point of the filled trigon.
\param y2 Y coordinate of the second point of the filled trigon.
\param x3 X coordinate of the third point of the filled trigon.
\param y3 Y coordinate of the third point of the filled trigon.
\param r The red value of the filled trigon to draw. 
\param g The green value of the filled trigon to draw. 
\param b The blue value of the filled trigon to draw. 
\param a The alpha value of the filled trigon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int filledTrigonRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Sint16 x3, Sint16 y3,
	Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	Sint16 vx[3];
	Sint16 vy[3];

	vx[0] = x1;
	vx[1] = x2;
	vx[2] = x3;
	vy[0] = y1;
	vy[1] = y2;
	vy[2] = y3;

	return (filledPolygonRGBA(renderer, vx, vy, 3, r, g, b, a));
}

/* ---- Polygon */

/*!
\brief Draw polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the polygon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int polygonColor(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color)
{
	return polygonRGBA(renderer, vx, vy, n, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw polygon with the currently set color and blend mode.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array. Minimum number is 3.

\returns Returns 0 on success, -1 on failure.
*/
static int polygon(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n)
{
	/*
	* Draw 
	*/
	int result = 0;
	int i, nn;
	SDL_FPoint *points;

	/*
	* Vertex array NULL check 
	*/
	if (vx == NULL) {
		return (-1);
	}
	if (vy == NULL) {
		return (-1);
	}

	/*
	* Sanity check 
	*/
	if (n < 3) {
		return (-1);
	}

	/*
	* Create array of points
	*/
	nn = n + 1;
	points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * nn);
	if (points == NULL) {
		return -1;
	}
	for (i = 0; i < n; i++) {
		points[i].x = (float)vx[i];
		points[i].y = (float)vy[i];
	}
	points[n].x = (float)vx[0];
	points[n].y = (float)vy[0];

	/*
	* Draw 
	*/
	result |= GFX_RESULT(SDL_RenderLines(renderer, points, nn));
	SDL_free(points);

	return (result);
}

/*!
\brief Draw polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the polygon to draw. 
\param g The green value of the polygon to draw. 
\param b The blue value of the polygon to draw. 
\param a The alpha value of the polygon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int polygonRGBA(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	/*
	* Draw 
	*/
	int result;

	/*
	* Vertex array NULL check 
	*/
	if (vx == NULL) {
		return (-1);
	}
	if (vy == NULL) {
		return (-1);
	}

	/*
	* Sanity check 
	*/
	if (n < 3) {
		return (-1);
	}

	/*
	* Set color 
	*/
	result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));

	/*
	* Draw 
	*/
	result |= polygon(renderer, vx, vy, n);

	return (result);
}

/* ---- AA-Polygon */

/*!
\brief Draw anti-aliased polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the aa-polygon.
\param vy Vertex array containing Y coordinates of the points of the aa-polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the aa-polygon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int aapolygonColor(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color)
{
	return aapolygonRGBA(renderer, vx, vy, n, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw anti-aliased polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the aa-polygon.
\param vy Vertex array containing Y coordinates of the points of the aa-polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the aa-polygon to draw. 
\param g The green value of the aa-polygon to draw. 
\param b The blue value of the aa-polygon to draw. 
\param a The alpha value of the aa-polygon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int aapolygonRGBA(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int i;
	const Sint16 *x1, *y1, *x2, *y2;
	GFX_Batch batch;

	/*
	* Vertex array NULL check 
	*/
	if (vx == NULL) {
		return (-1);
	}
	if (vy == NULL) {
		return (-1);
	}

	/*
	* Sanity check 
	*/
	if (n < 3) {
		return (-1);
	}

	/*
	* Pointer setup 
	*/
	x1 = x2 = vx;
	y1 = y2 = vy;
	x2++;
	y2++;

	/*
	* Draw (all edges in one batch)
	*/
	_gfxBatchBegin(&batch, renderer, r, g, b, a);
	for (i = 1; i < n; i++) {
		_aalineBatch(&batch, *x1, *y1, *x2, *y2, 0);
		x1 = x2;
		y1 = y2;
		x2++;
		y2++;
	}

	_aalineBatch(&batch, *x1, *y1, *vx, *vy, 0);

	return (_gfxBatchEnd(&batch));
}

/* ---- Filled Polygon */

/*!
\brief Internal helper qsort callback functions used in filled polygon drawing.

\param a Pointer to the first intersection.
\param b Pointer to the second intersection.

\returns Returns 0 if a==b, a negative number if a<b or a positive number if a>b.
*/
static int _gfxPrimitivesCompareInt(const void *a, const void *b)
{
	const int ia = *(const int *)a;
	const int ib = *(const int *)b;

	/* No subtraction: the difference of two 16.16 intersections can overflow (PRIM-007) */
	return (ia > ib) - (ia < ib);
}

/*!
\brief Number of edge intersections that fit into the stack buffer of the polygon fill functions.
*/
#define GFX_POLY_STACK_INTS 64

/*!
\brief Internal helper to get a buffer for the edge intersections of a polygon with n vertices.

Without a caller-provided cache (polyInts or polyAllocated NULL) the stack buffer is used for small
polygons and a temporary buffer is allocated for larger ones (no global state, PRIM-002). A
caller-provided cache is grown with realloc as needed and remains owned by the caller.

\param n Number of vertices of the polygon.
\param stackInts Stack buffer with GFX_POLY_STACK_INTS entries.
\param polyInts Optional caller-provided cache.
\param polyAllocated Optional size of the caller-provided cache.

\returns The buffer or NULL if out of memory.
*/
static int *_gfxPrimitivesPolyBuffer(int n, int *stackInts, int **polyInts, int *polyAllocated)
{
	int *buffer;

	if (n > (int)(SDL_MAX_SINT32 / sizeof(int))) {
		return NULL;
	}
	if ((polyInts == NULL) || (polyAllocated == NULL)) {
		if (n <= GFX_POLY_STACK_INTS) {
			return stackInts;
		}
		return (int *)SDL_malloc(sizeof(int) * (size_t)n);
	}
	if ((*polyAllocated >= n) && (*polyInts != NULL)) {
		return *polyInts;
	}
	buffer = (int *)SDL_realloc((*polyAllocated > 0) ? *polyInts : NULL, sizeof(int) * (size_t)n);
	if (buffer == NULL) {
		/* The caller's cache is unchanged and remains valid (PRIM-006) */
		return NULL;
	}
	*polyInts = buffer;
	*polyAllocated = n;
	return buffer;
}

/*!
\brief Internal helper to release a buffer returned by _gfxPrimitivesPolyBuffer.
*/
static void _gfxPrimitivesPolyBufferFree(int *buffer, const int *stackInts, int **polyInts, int *polyAllocated)
{
	if (((polyInts == NULL) || (polyAllocated == NULL)) && (buffer != stackInts)) {
		SDL_free(buffer);
	}
}

/*!
\brief Internal helper to get the range of rows and columns visible in the viewport and clip rectangle.

Drawing coordinates are relative to the viewport and in logical (unscaled) units, like the rectangles
returned by SDL_GetRenderViewport and SDL_GetRenderClipRect. One extra row/column on each side covers
rounding with fractional scale factors; SDL clips anything drawn outside anyway (PERF-001).

\param renderer The renderer to draw on.
\param xmin Pointer to the first visible column.
\param ymin Pointer to the first visible row.
\param xmax Pointer to the last visible column.
\param ymax Pointer to the last visible row.
*/
static void _gfxPrimitivesVisibleArea(SDL_Renderer *renderer, int *xmin, int *ymin, int *xmax, int *ymax)
{
	SDL_Rect viewport, clip;

	*xmin = *ymin = -32768;
	*xmax = *ymax = 32767;
	if (SDL_GetRenderViewport(renderer, &viewport) && (viewport.w > 0) && (viewport.h > 0)) {
		*xmin = -1;
		*ymin = -1;
		*xmax = viewport.w;
		*ymax = viewport.h;
	}
	if (SDL_GetRenderClipRect(renderer, &clip) && (clip.w > 0) && (clip.h > 0)) {
		*xmin = SDL_max(*xmin, clip.x - 1);
		*ymin = SDL_max(*ymin, clip.y - 1);
		*xmax = SDL_min(*xmax, clip.x + clip.w);
		*ymax = SDL_min(*ymax, clip.y + clip.h);
	}
}
/*!
\brief Internal helper to compute the sorted intersections of the polygon edges with row y.

The intersections are 16.16 fixed point x coordinates. The intermediate products use 64-bit math
since they overflow int for polygons spanning more than 32767 pixels (PRIM-007); the results are
between the edge endpoints and fit into an int.

\param vx Vertex array containing X coordinates of the points of the polygon.
\param vy Vertex array containing Y coordinates of the points of the polygon.
\param n Number of points in the vertex array.
\param y The row.
\param maxy The largest Y coordinate of the polygon.
\param ints Buffer for at least n intersections.

\returns The number of intersections.
*/
static int _gfxPrimitivesPolyIntersections(const Sint16 *vx, const Sint16 *vy, int n, int y, int maxy, int *ints)
{
	int i, ind1, ind2;
	int x1, y1, x2, y2;
	int count = 0;

	for (i = 0; i < n; i++) {
		if (!i) {
			ind1 = n - 1;
			ind2 = 0;
		} else {
			ind1 = i - 1;
			ind2 = i;
		}
		y1 = vy[ind1];
		y2 = vy[ind2];
		if (y1 < y2) {
			x1 = vx[ind1];
			x2 = vx[ind2];
		} else if (y1 > y2) {
			y2 = vy[ind1];
			y1 = vy[ind2];
			x2 = vx[ind1];
			x1 = vx[ind2];
		} else {
			continue;
		}
		if (((y >= y1) && (y < y2)) || ((y == maxy) && (y > y1) && (y <= y2))) {
			ints[count++] = (int)((((Sint64)65536 * (y - y1)) / (y2 - y1)) * (x2 - x1) + (Sint64)65536 * x1);
		}
	}

	qsort(ints, count, sizeof(int), _gfxPrimitivesCompareInt);

	return count;
}

/*!
\brief Internal helper to round a 16.16 intersection (moved inwards by offset) to a pixel coordinate.
*/
static Sint16 _gfxPrimitivesPolyRound(int v, int offset)
{
	const Sint64 w = (Sint64)v + offset;

	return (Sint16)((w >> 16) + ((w & 32768) >> 15));
}

/*!
\brief Draw filled polygon with alpha blending (multi-threaded capable).

Note: The last two parameters are optional. Without them a temporary buffer is used, which is thread-safe as well.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the filled polygon to draw. 
\param g The green value of the filled polygon to draw. 
\param b The blue value of the filled polygon to draw. 
\param a The alpha value of the filled polygon to draw.
\param polyInts Optional cache for the intersection buffer (initialize to NULL; release with SDL_free()), or NULL.
\param polyAllocated Optional size of the cache (initialize to 0), or NULL.

\returns Returns 0 on success, -1 on failure.
*/
static int filledPolygonRGBAMT(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a, int **polyInts, int *polyAllocated)
{
	int i, y, ints;
	int miny, maxy, ystart, yend;
	int xa, xb, tmp, prevx, visxmin, visxmax;
	int stackInts[GFX_POLY_STACK_INTS];
	int *buffer;
	GFX_Batch batch;

	/*
	* Parameter checks
	*/
	if ((renderer == NULL) || (vx == NULL) || (vy == NULL)) {
		return (-1);
	}
	if (n < 3) {
		return -1;
	}

	/*
	* Determine Y maxima 
	*/
	miny = vy[0];
	maxy = vy[0];
	for (i = 1; (i < n); i++) {
		if (vy[i] < miny) {
			miny = vy[i];
		} else if (vy[i] > maxy) {
			maxy = vy[i];
		}
	}

	/*
	* Set color 
	*/
	_gfxBatchBegin(&batch, renderer, r, g, b, a);

	/*
	* Only scan visible rows
	*/
	_gfxPrimitivesVisibleArea(renderer, &visxmin, &ystart, &visxmax, &yend);
	ystart = SDL_max(ystart, miny);
	yend = SDL_min(yend, maxy);
	if (ystart > yend) {
		return (_gfxBatchEnd(&batch));
	}

	buffer = _gfxPrimitivesPolyBuffer(n, stackInts, polyInts, polyAllocated);
	if (buffer == NULL) {
		return (-1);
	}

	/*
	* Draw, scanning y 
	*/
	for (y = ystart; (y <= yend); y++) {
		ints = _gfxPrimitivesPolyIntersections(vx, vy, n, y, maxy, buffer);
		prevx = -0x7FFFFFFF;
		for (i = 0; (i + 1 < ints); i += 2) {
			xa = _gfxPrimitivesPolyRound(buffer[i], 1);
			xb = _gfxPrimitivesPolyRound(buffer[i + 1], -1);
			if (xa > xb) {
				tmp = xa;
				xa = xb;
				xb = tmp;
			}
			/* Spans meeting at a vertex share a pixel: draw it once (PRIM-016) */
			xa = SDL_max(xa, prevx + 1);
			prevx = SDL_max(prevx, xb);
			/* Skip invisible parts; the texture offset is derived from the start column */
			xa = SDL_max(xa, visxmin);
			xb = SDL_min(xb, visxmax);
			if (xa > xb) {
				continue;
			}
			_gfxBatchHLine(&batch, (Sint16)xa, (Sint16)xb, (Sint16)y);
		}
	}

	_gfxPrimitivesPolyBufferFree(buffer, stackInts, polyInts, polyAllocated);

	return (_gfxBatchEnd(&batch));
}
/*!
\brief Draw filled polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param color The color value of the filled polygon to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int filledPolygonColor(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint32 color)
{
	return filledPolygonRGBAMT(renderer, vx, vy, n, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color), NULL, NULL);
}

/*!
\brief Draw filled polygon with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the filled polygon.
\param vy Vertex array containing Y coordinates of the points of the filled polygon.
\param n Number of points in the vertex array. Minimum number is 3.
\param r The red value of the filled polygon to draw. 
\param g The green value of the filled polygon to draw. 
\param b The blue value of the filed polygon to draw. 
\param a The alpha value of the filled polygon to draw.

\returns Returns 0 on success, -1 on failure.
*/
int filledPolygonRGBA(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return filledPolygonRGBAMT(renderer, vx, vy, n, r, g, b, a, NULL, NULL);
}

/* ---- Textured Polygon */

/*!
\brief Internal function to copy a part of a texture with integer source and destination rectangles.

\returns Returns 0 on success, -1 on failure.
*/
static int _gfxRenderTexture(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect)
{
	SDL_FRect fsrc, fdst;

	SDL_RectToFRect(srcrect, &fsrc);
	SDL_RectToFRect(dstrect, &fdst);
	return GFX_RESULT(SDL_RenderTexture(renderer, texture, &fsrc, &fdst));
}

/*!
\brief Internal function to draw a textured horizontal line.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point (i.e. left) of the line.
\param x2 X coordinate of the second point (i.e. right) of the line.
\param y Y coordinate of the points of the line.
\param texture The texture to retrieve color information from.
\param texture_w The width of the texture.
\param texture_h The height of the texture.
\param texture_dx The X offset for the texture lookup.
\param texture_dy The Y offset for the textured lookup.

\returns Returns 0 on success, -1 on failure.
*/
static int _HLineTextured(SDL_Renderer *renderer, Sint16 x1, Sint16 x2, Sint16 y, SDL_Texture *texture, int texture_w, int texture_h, int texture_dx, int texture_dy)
{
	int w;
	Sint16 xtmp;
	int result = 0;
	int texture_x_walker;
	int texture_y_start;
	SDL_Rect source_rect, dst_rect;
	int pixels_written, write_width;

	/*
	* Swap x1, x2 if required to ensure x1<=x2
	*/
	if (x1 > x2) {
		xtmp = x1;
		x1 = x2;
		x2 = xtmp;
	}

	/*
	* Calculate width to draw
	*/
	w = x2 - x1 + 1;

	/*
	* Determine where in the texture we start drawing
	*/
	texture_x_walker = (x1 - texture_dx) % texture_w;
	if (texture_x_walker < 0) {
		texture_x_walker = texture_w + texture_x_walker;
	}

	texture_y_start = (y + texture_dy) % texture_h;
	if (texture_y_start < 0) {
		texture_y_start = texture_h + texture_y_start;
	}

	/* setup the source rectangle; we are only drawing one horizontal line */
	source_rect.y = texture_y_start;
	source_rect.x = texture_x_walker;
	source_rect.h = 1;

	/* we will draw to the current y */
	dst_rect.y = y;
	dst_rect.h = 1;

	/* if there are enough pixels left in the current row of the texture */
	/* draw it all at once */
	if (w <= texture_w - texture_x_walker) {
		source_rect.w = w;
		source_rect.x = texture_x_walker;
		dst_rect.x = x1;
		dst_rect.w = source_rect.w;
		result = _gfxRenderTexture(renderer, texture, &source_rect, &dst_rect);
	} else {
		/* we need to draw multiple times */
		/* draw the first segment */
		pixels_written = texture_w - texture_x_walker;
		source_rect.w = pixels_written;
		source_rect.x = texture_x_walker;
		dst_rect.x = x1;
		dst_rect.w = source_rect.w;
		result |= _gfxRenderTexture(renderer, texture, &source_rect, &dst_rect);
		write_width = texture_w;

		/* now draw the rest */
		/* set the source x to 0 */
		source_rect.x = 0;
		while (pixels_written < w) {
			if (write_width >= w - pixels_written) {
				write_width = w - pixels_written;
			}
			source_rect.w = write_width;
			dst_rect.x = x1 + pixels_written;
			dst_rect.w = source_rect.w;
			result |= _gfxRenderTexture(renderer, texture, &source_rect, &dst_rect);
			pixels_written += write_width;
		}
	}

	return result;
}

/*!
\brief Draws a polygon filled with the given texture (Multi-Threading Capable). 

\param renderer The renderer to draw on.
\param vx array of x vector components
\param vy array of x vector components
\param n the amount of vectors in the vx and vy array
\param texture the sdl surface to use to fill the polygon
\param texture_dx Horizontal texture offset: the pixel at x uses texture column (x - texture_dx) modulo the texture width.
\param texture_dy Vertical texture offset: the pixel at y uses texture row (y + texture_dy) modulo the texture height.
Note the different signs, kept for compatibility (PRIM-014). To move the texture together with a polygon moved
by (mx, my), add mx to texture_dx and subtract my from texture_dy.
\param polyInts Optional cache for the intersection buffer (initialize to NULL; release with SDL_free()), or NULL.
\param polyAllocated Optional size of the cache (initialize to 0), or NULL.

\returns Returns 0 on success, -1 on failure.
*/
static int texturedPolygonMT(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n,
	SDL_Surface *texture, int texture_dx, int texture_dy, int **polyInts, int *polyAllocated)
{
	int result;
	int i, y, ints;
	int miny, maxy, ystart, yend;
	int xa, xb, tmp, prevx, visxmin, visxmax;
	int stackInts[GFX_POLY_STACK_INTS];
	int *buffer;
	SDL_Texture *textureAsTexture = NULL;

	/*
	* Parameter checks
	*/
	if ((renderer == NULL) || (vx == NULL) || (vy == NULL) || (texture == NULL)) {
		return (-1);
	}
	if (n < 3) {
		return -1;
	}

	/*
	* Determine Y maxima 
	*/
	miny = vy[0];
	maxy = vy[0];
	for (i = 1; (i < n); i++) {
		if (vy[i] < miny) {
			miny = vy[i];
		} else if (vy[i] > maxy) {
			maxy = vy[i];
		}
	}

	/*
	* Only scan visible rows
	*/
	_gfxPrimitivesVisibleArea(renderer, &visxmin, &ystart, &visxmax, &yend);
	ystart = SDL_max(ystart, miny);
	yend = SDL_min(yend, maxy);
	if (ystart > yend) {
		return (0);
	}

	buffer = _gfxPrimitivesPolyBuffer(n, stackInts, polyInts, polyAllocated);
	if (buffer == NULL) {
		return (-1);
	}

	/* Create texture for drawing */
	textureAsTexture = SDL_CreateTextureFromSurface(renderer, texture);
	if (textureAsTexture == NULL) {
		_gfxPrimitivesPolyBufferFree(buffer, stackInts, polyInts, polyAllocated);
		return -1;
	}
	SDL_SetTextureBlendMode(textureAsTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(textureAsTexture, SDL_SCALEMODE_NEAREST);

	/*
	* Draw, scanning y 
	*/
	result = 0;
	for (y = ystart; (y <= yend); y++) {
		ints = _gfxPrimitivesPolyIntersections(vx, vy, n, y, maxy, buffer);
		prevx = -0x7FFFFFFF;
		for (i = 0; (i + 1 < ints); i += 2) {
			xa = _gfxPrimitivesPolyRound(buffer[i], 1);
			xb = _gfxPrimitivesPolyRound(buffer[i + 1], -1);
			if (xa > xb) {
				tmp = xa;
				xa = xb;
				xb = tmp;
			}
			/* Spans meeting at a vertex share a pixel: draw it once (PRIM-016) */
			xa = SDL_max(xa, prevx + 1);
			prevx = SDL_max(prevx, xb);
			/* Skip invisible parts; the texture offset is derived from the start column */
			xa = SDL_max(xa, visxmin);
			xb = SDL_min(xb, visxmax);
			if (xa > xb) {
				continue;
			}
			result |= _HLineTextured(renderer, (Sint16)xa, (Sint16)xb, (Sint16)y, textureAsTexture, texture->w, texture->h, texture_dx, texture_dy);
		}
	}

	SDL_DestroyTexture(textureAsTexture);
	_gfxPrimitivesPolyBufferFree(buffer, stackInts, polyInts, polyAllocated);

	return (result);
}
/*!
\brief Draws a polygon filled with the given texture. 

This standard version is calling multithreaded versions with NULL cache parameters.

\param renderer The renderer to draw on.
\param vx array of x vector components
\param vy array of x vector components
\param n the amount of vectors in the vx and vy array
\param texture the sdl surface to use to fill the polygon
\param texture_dx Horizontal texture offset: the pixel at x uses texture column (x - texture_dx) modulo the texture width.
\param texture_dy Vertical texture offset: the pixel at y uses texture row (y + texture_dy) modulo the texture height.
Note the different signs, kept for compatibility (PRIM-014). To move the texture together with a polygon moved
by (mx, my), add mx to texture_dx and subtract my from texture_dy.

\returns Returns 0 on success, -1 on failure.
*/
int texturedPolygon(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, SDL_Surface *texture, int texture_dx, int texture_dy)
{
	/*
	* Draw
	*/
	return (texturedPolygonMT(renderer, vx, vy, n, texture, texture_dx, texture_dy, NULL, NULL));
}

/* ---- Character */

/*!
\brief Global cache for NxM pixel font textures created at runtime.

The textures belong to gfxPrimitivesFontRenderer. When text is drawn with another renderer the cache
is reset without destroying the textures (they are released by SDL together with their renderer).
*/
static SDL_Texture *gfxPrimitivesFont[256];

/*!
\brief Renderer that owns the textures in gfxPrimitivesFont.
*/
static SDL_Renderer *gfxPrimitivesFontRenderer = NULL;

/*!
\brief Resets the glyph texture cache.

\param destroy Destroy the cached textures (only valid while their renderer exists) or just forget them.
*/
static void _gfxPrimitivesFontCacheReset(int destroy)
{
	int i;
	for (i = 0; i < 256; i++) {
		if (gfxPrimitivesFont[i] && destroy) {
			SDL_DestroyTexture(gfxPrimitivesFont[i]);
		}
		gfxPrimitivesFont[i] = NULL;
	}
	if (destroy) {
		gfxPrimitivesFontRenderer = NULL;
	}
}

/*!
\brief Pointer to the current font data. Default is a 8x8 pixel internal font. 
*/
static const unsigned char *currentFontdata = gfxPrimitivesFontdata;

/*!
\brief Width of the current font. Default is 8. 
*/
static Uint32 charWidth = 8;

/*!
\brief Height of the current font. Default is 8. 
*/
static Uint32 charHeight = 8;

/*!
\brief Width for rendering. Autocalculated.
*/
static Uint32 charWidthLocal = 8;

/*!
\brief Height for rendering. Autocalculated.
*/
static Uint32 charHeightLocal = 8;

/*!
\brief Pitch of the current font in bytes. Default is 1. 
*/
static Uint32 charPitch = 1;

/*!
\brief Characters 90deg clockwise rotations. Default is 0. Max is 3. 
*/
static Uint32 charRotation = 0;

/*!
\brief Character data size in bytes of the current font. Default is 8. 
*/
static Uint32 charSize = 8;

/*!
\brief Largest glyph width or height accepted by gfxPrimitivesSetFont.
*/
#define GFX_FONT_MAX_SIZE 4096

/*!
\brief Sets or resets the current global font data.

The font data array is organized in follows: 
[fontdata] = [character 0][character 1]...[character 255] where
[character n] = [byte 1 row 1][byte 2 row 1]...[byte {pitch} row 1][byte 1 row 2] ...[byte {pitch} row height] where
[byte n] = [bit 0]...[bit 7] where 
[bit n] = [0 for transparent pixel|1 for colored pixel]

\param fontdata Pointer to array of font data. Set to NULL, to reset global font to the default 8x8 font.
The data is used directly (not copied) and must stay valid while the font is set.
\param cw Width of a character in pixels (1 to 4096). Ignored if fontdata==NULL.
\param ch Height of a character in pixels (1 to 4096). Ignored if fontdata==NULL. Invalid sizes select the built-in 8x8 font.

Glyphs are cached as textures of the renderer that draws text. Calling this function releases the
cache: call gfxPrimitivesSetFont(NULL, 0, 0) before destroying a renderer that was used to draw text.
*/
void gfxPrimitivesSetFont(const void *fontdata, Uint32 cw, Uint32 ch)
{
	/* Glyphs larger than GFX_FONT_MAX_SIZE are rejected like a NULL font (size math stays in range) */
	if ((fontdata) && (cw) && (ch) && (cw <= GFX_FONT_MAX_SIZE) && (ch <= GFX_FONT_MAX_SIZE)) {
		currentFontdata = (unsigned char *)fontdata;
		charWidth = cw;
		charHeight = ch;
	} else {
		currentFontdata = gfxPrimitivesFontdata;
		charWidth = 8;
		charHeight = 8;
	}

	charPitch = (charWidth + 7) / 8;
	charSize = charPitch * charHeight;

	/* Maybe flip width/height for rendering */
	if ((charRotation == 1) || (charRotation == 3)) {
		charWidthLocal = charHeight;
		charHeightLocal = charWidth;
	} else {
		charWidthLocal = charWidth;
		charHeightLocal = charHeight;
	}

	/* Release the character cache */
	_gfxPrimitivesFontCacheReset(1);
}

/*!
\brief Sets current global font character rotation steps. 

Default is 0 (no rotation). 1 = 90deg clockwise. 2 = 180deg clockwise. 3 = 270deg clockwise.
Changing the rotation, will reset the character cache.

\param rotation Number of 90deg clockwise steps to rotate
*/
void gfxPrimitivesSetFontRotation(Uint32 rotation)
{
	rotation = rotation & 3;
	if (charRotation != rotation) {
		/* Store rotation */
		charRotation = rotation;

		/* Maybe flip width/height for rendering */
		if ((charRotation == 1) || (charRotation == 3)) {
			charWidthLocal = charHeight;
			charHeightLocal = charWidth;
		} else {
			charWidthLocal = charWidth;
			charHeightLocal = charHeight;
		}

		/* Release the character cache */
		_gfxPrimitivesFontCacheReset(1);
	}
}

/*!
\brief Draw a character of the currently set font.

\param renderer The Renderer to draw on.
\param x X (horizontal) coordinate of the upper left corner of the character.
\param y Y (vertical) coordinate of the upper left corner of the character.
\param c The character to draw.
\param r The red value of the character to draw. 
\param g The green value of the character to draw. 
\param b The blue value of the character to draw. 
\param a The alpha value of the character to draw.

\returns Returns 0 on success, -1 on failure.
*/
int characterRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, char c, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	SDL_Rect srect;
	SDL_Rect drect;
	int result;
	Uint32 ix, iy;
	const unsigned char *charpos;
	Uint8 *curpos;
	Uint8 patt, mask;
	Uint8 *linepos;
	Uint32 pitch;
	SDL_Surface *character;
	SDL_Surface *rotatedCharacter;
	Uint32 ci;

	/*
	* Setup source rectangle
	*/
	srect.x = 0;
	srect.y = 0;
	srect.w = charWidthLocal;
	srect.h = charHeightLocal;

	/*
	* Setup destination rectangle
	*/
	drect.x = x;
	drect.y = y;
	drect.w = charWidthLocal;
	drect.h = charHeightLocal;

	/* The cached textures belong to one renderer (FONT-001) */
	if (renderer != gfxPrimitivesFontRenderer) {
		_gfxPrimitivesFontCacheReset(0);
		gfxPrimitivesFontRenderer = renderer;
	}

	/* Character index in cache */
	ci = (unsigned char)c;

	/*
	* Create new charWidth x charHeight bitmap surface if not already present.
	* Might get rotated later.
	*/
	if (gfxPrimitivesFont[ci] == NULL) {
		/*
		* Redraw character into surface
		*/
		character = SDL_CreateSurface((int)charWidth, (int)charHeight, SDL_PIXELFORMAT_RGBA8888);
		if (character == NULL) {
			return (-1);
		}

		charpos = currentFontdata + (size_t)ci * charSize;
		linepos = (Uint8 *)character->pixels;
		pitch = character->pitch;

		/*
		* Drawing loop 
		*/
		patt = 0;
		for (iy = 0; iy < charHeight; iy++) {
			mask = 0x00;
			curpos = linepos;
			for (ix = 0; ix < charWidth; ix++) {
				if (!(mask >>= 1)) {
					patt = *charpos++;
					mask = 0x80;
				}
				if (patt & mask) {
					*(Uint32 *)curpos = 0xffffffff;
				} else {
					*(Uint32 *)curpos = 0;
				}
				curpos += 4;
			}
			linepos += pitch;
		}

		/* Maybe rotate and replace cached image */
		if (charRotation > 0) {
			rotatedCharacter = rotateSurface90Degrees(character, charRotation);
			SDL_DestroySurface(character);
			character = rotatedCharacter;
		}

		/* Convert temp surface into texture */
		gfxPrimitivesFont[ci] = SDL_CreateTextureFromSurface(renderer, character);
		SDL_DestroySurface(character);

		/*
		* Check pointer 
		*/
		if (gfxPrimitivesFont[ci] == NULL) {
			return (-1);
		}
		SDL_SetTextureScaleMode(gfxPrimitivesFont[ci], SDL_SCALEMODE_NEAREST);
	}

	/*
	* Set color 
	*/
	result = 0;
	result |= GFX_RESULT(SDL_SetTextureColorMod(gfxPrimitivesFont[ci], r, g, b));
	result |= GFX_RESULT(SDL_SetTextureAlphaMod(gfxPrimitivesFont[ci], a));

	/*
	* Draw texture onto destination 
	*/
	result |= _gfxRenderTexture(renderer, gfxPrimitivesFont[ci], &srect, &drect);

	return (result);
}


/*!
\brief Draw a character of the currently set font.

\param renderer The renderer to draw on.
\param x X (horizontal) coordinate of the upper left corner of the character.
\param y Y (vertical) coordinate of the upper left corner of the character.
\param c The character to draw.
\param color The color value of the character to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int characterColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, char c, Uint32 color)
{
	return characterRGBA(renderer, x, y, c, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}


/*!
\brief Draw a string in the currently set font.

The spacing between consequtive characters in the string is the fixed number of pixels 
of the character width of the current global font.

\param renderer The renderer to draw on.
\param x X (horizontal) coordinate of the upper left corner of the string.
\param y Y (vertical) coordinate of the upper left corner of the string.
\param s The string to draw.
\param color The color value of the string to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int stringColor(SDL_Renderer *renderer, Sint16 x, Sint16 y, const char *s, Uint32 color)
{
	return stringRGBA(renderer, x, y, s, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw a string in the currently set font.

\param renderer The renderer to draw on.
\param x X (horizontal) coordinate of the upper left corner of the string.
\param y Y (vertical) coordinate of the upper left corner of the string.
\param s The string to draw.
\param r The red value of the string to draw. 
\param g The green value of the string to draw. 
\param b The blue value of the string to draw. 
\param a The alpha value of the string to draw.

\returns Returns 0 on success, -1 on failure.
*/
int stringRGBA(SDL_Renderer *renderer, Sint16 x, Sint16 y, const char *s, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result = 0;
	Sint16 curx = x;
	Sint16 cury = y;
	const char *curchar = s;

	while (*curchar && !result) {
		result |= characterRGBA(renderer, curx, cury, *curchar, r, g, b, a);
		switch (charRotation) {
			case 0:
				curx = (Sint16)(curx + (int)charWidthLocal);
				break;
			case 2:
				curx = (Sint16)(curx - (int)charWidthLocal);
				break;
			case 1:
				cury = (Sint16)(cury + (int)charHeightLocal);
				break;
			case 3:
				cury = (Sint16)(cury - (int)charHeightLocal);
				break;
			default:
				break;
		}
		curchar++;
	}

	return (result);
}

/* ---- Bezier curve */

/*!
\brief Internal function to calculate bezier interpolator of data array with ndata values at position 't'.

\param data Array of values.
\param ndata Size of array.
\param t Position for which to calculate interpolated value. t should be between [0, ndata].

\returns Interpolated value at position t, value[0] when t<0, value[n-1] when t>n.
*/
static double _evaluateBezier(double *data, int ndata, double t)
{
	double mu, result;
	int n, k, kn, nn, nkn;
	double blend, muk, munk;

	/* Sanity check bounds */
	if (t < 0.0) {
		return (data[0]);
	}
	if (t >= (double)ndata) {
		return (data[ndata - 1]);
	}

	/* Adjust t to the range 0.0 to 1.0 */
	mu = t / (double)ndata;

	/* Calculate interpolate */
	n = ndata - 1;
	result = 0.0;
	muk = 1;
	munk = pow(1 - mu, (double)n);
	for (k = 0; k <= n; k++) {
		nn = n;
		kn = k;
		nkn = n - k;
		blend = muk * munk;
		muk *= mu;
		munk /= (1 - mu);
		while (nn >= 1) {
			blend *= nn;
			nn--;
			if (kn > 1) {
				blend /= (double)kn;
				kn--;
			}
			if (nkn > 1) {
				blend /= (double)nkn;
				nkn--;
			}
		}
		result += data[k] * blend;
	}

	return (result);
}

/*!
\brief Draw a bezier curve with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the bezier curve.
\param vy Vertex array containing Y coordinates of the points of the bezier curve.
\param n Number of points in the vertex array. Minimum number is 3.
\param s Number of steps for the interpolation. Minimum number is 2.
\param color The color value of the bezier curve to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int bezierColor(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, int s, Uint32 color)
{
	return bezierRGBA(renderer, vx, vy, n, s, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw a bezier curve with alpha blending.

\param renderer The renderer to draw on.
\param vx Vertex array containing X coordinates of the points of the bezier curve.
\param vy Vertex array containing Y coordinates of the points of the bezier curve.
\param n Number of points in the vertex array. Minimum number is 3.
\param s Number of steps for the interpolation. Minimum number is 2.
\param r The red value of the bezier curve to draw. 
\param g The green value of the bezier curve to draw. 
\param b The blue value of the bezier curve to draw. 
\param a The alpha value of the bezier curve to draw.

\returns Returns 0 on success, -1 on failure.
*/
int bezierRGBA(SDL_Renderer *renderer, const Sint16 *vx, const Sint16 *vy, int n, int s, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int result;
	int i;
	double *x, *y, t, stepsize;
	Sint16 x2, y2;
	SDL_FPoint *points;
	int npoints;

	/*
	* Sanity check 
	*/
	if (n < 3) {
		return (-1);
	}
	if (s < 2) {
		return (-1);
	}

	/*
	* Variable setup 
	*/
	stepsize = (double)1.0 / (double)s;

	/* Transfer vertices into float arrays */
	if ((x = (double *)SDL_malloc(sizeof(double) * (n + 1))) == NULL) {
		return (-1);
	}
	if ((y = (double *)SDL_malloc(sizeof(double) * (n + 1))) == NULL) {
		SDL_free(x);
		return (-1);
	}
	if ((points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * ((size_t)n * (size_t)s + 2))) == NULL) {
		SDL_free(x);
		SDL_free(y);
		return (-1);
	}
	for (i = 0; i < n; i++) {
		x[i] = (double)vx[i];
		y[i] = (double)vy[i];
	}
	x[n] = (double)vx[0];
	y[n] = (double)vy[0];

	/*
	* Set color 
	*/
	result = 0;
	result |= GFX_RESULT(SDL_SetRenderDrawBlendMode(renderer, (a == 255) ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND));
	result |= GFX_RESULT(SDL_SetRenderDrawColor(renderer, r, g, b, a));

	/*
	* Draw 
	*/
	/* One polyline: consecutive segments share their end points, which must be drawn once (PRIM-016) */
	t = 0.0;
	points[0].x = (float)(Sint16)lrint(_evaluateBezier(x, n + 1, t));
	points[0].y = (float)(Sint16)lrint(_evaluateBezier(y, n + 1, t));
	npoints = 1;
	for (i = 0; i <= (n * s); i++) {
		t += stepsize;
		x2 = (Sint16)_evaluateBezier(x, n, t);
		y2 = (Sint16)_evaluateBezier(y, n, t);
		if (((float)x2 != points[npoints - 1].x) || ((float)y2 != points[npoints - 1].y)) {
			points[npoints].x = (float)x2;
			points[npoints].y = (float)y2;
			npoints++;
		}
	}
	if (npoints == 1) {
		result |= GFX_RESULT(SDL_RenderPoint(renderer, points[0].x, points[0].y));
	} else {
		result |= GFX_RESULT(SDL_RenderLines(renderer, points, npoints));
	}

	/* Clean up temporary array */
	SDL_free(points);
	SDL_free(x);
	SDL_free(y);

	return (result);
}


/*!
\brief Draw a thick line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the line.
\param y1 Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param width Width of the line in pixels. Must be >0.
\param color The color value of the line to draw (0xRRGGBBAA). 

\returns Returns 0 on success, -1 on failure.
*/
int thickLineColor(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 width, Uint32 color)
{
	return thickLineRGBA(renderer, x1, y1, x2, y2, width, GFX_COLOR_R(color), GFX_COLOR_G(color), GFX_COLOR_B(color), GFX_COLOR_A(color));
}

/*!
\brief Draw a thick line with alpha blending.

\param renderer The renderer to draw on.
\param x1 X coordinate of the first point of the line.
\param y1 Y coordinate of the first point of the line.
\param x2 X coordinate of the second point of the line.
\param y2 Y coordinate of the second point of the line.
\param width Width of the line in pixels. Must be >0.
\param r The red value of the character to draw. 
\param g The green value of the character to draw. 
\param b The blue value of the character to draw. 
\param a The alpha value of the character to draw.

\returns Returns 0 on success, -1 on failure.
*/
int thickLineRGBA(SDL_Renderer *renderer, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Uint8 width, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	int wh;
	double dx, dy, dx1, dy1, dx2, dy2;
	double l, wl2, nx, ny, ang, adj;
	Sint16 px[4], py[4];

	if (renderer == NULL) {
		return -1;
	}

	if (width < 1) {
		return -1;
	}

	/* Special case: thick "point" */
	if ((x1 == x2) && (y1 == y2)) {
		/* A width x width square centered on the point (PRIM-010) */
		wh = width / 2;
		return boxRGBA(renderer, (Sint16)(x1 - wh), (Sint16)(y1 - wh), (Sint16)(x1 - wh + width - 1), (Sint16)(y1 - wh + width - 1), r, g, b, a);
	}

	/* Special case: width == 1 */
	if (width == 1) {
		return lineRGBA(renderer, x1, y1, x2, y2, r, g, b, a);
	}

	/* Calculate offsets for sides */
	dx = (double)(x2 - x1);
	dy = (double)(y2 - y1);
	l = SDL_sqrt(dx * dx + dy * dy);
	ang = SDL_atan2(dx, dy);
	adj = 0.1 + 0.9 * SDL_fabs(SDL_cos(2.0 * ang));
	wl2 = ((double)width - adj) / (2.0 * l);
	nx = dx * wl2;
	ny = dy * wl2;

	/* Build polygon */
	dx1 = (double)x1;
	dy1 = (double)y1;
	dx2 = (double)x2;
	dy2 = (double)y2;
	px[0] = (Sint16)(dx1 + ny);
	px[1] = (Sint16)(dx1 - ny);
	px[2] = (Sint16)(dx2 - ny);
	px[3] = (Sint16)(dx2 + ny);
	py[0] = (Sint16)(dy1 - nx);
	py[1] = (Sint16)(dy1 + nx);
	py[2] = (Sint16)(dy2 + nx);
	py[3] = (Sint16)(dy2 - nx);

	/* Draw polygon */
	return filledPolygonRGBA(renderer, px, py, 4, r, g, b, a);
}
