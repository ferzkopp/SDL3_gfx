/* Minimal consumer of an installed SDL3_gfx: verifies headers, linking and a basic draw call. */
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3_gfx/SDL3_framerate.h>
#include <SDL3_gfx/SDL3_gfxPrimitives.h>
#include <SDL3_gfx/SDL3_imageFilter.h>
#include <SDL3_gfx/SDL3_rotozoom.h>

int main(int argc, char *argv[])
{
	SDL_Surface *surface;
	SDL_Renderer *renderer;
	Uint32 pixel;
	int result = 1;

	(void)argc;
	(void)argv;

	surface = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_ARGB8888);
	if (surface == NULL) {
		fprintf(stderr, "SDL_CreateSurface: %s\n", SDL_GetError());
		return 1;
	}
	renderer = SDL_CreateSoftwareRenderer(surface);
	if (renderer == NULL) {
		fprintf(stderr, "SDL_CreateSoftwareRenderer: %s\n", SDL_GetError());
		SDL_DestroySurface(surface);
		return 1;
	}

	if (pixelRGBA(renderer, 3, 4, 255, 0, 0, 255) == 0) {
		SDL_RenderPresent(renderer);
		pixel = ((Uint32 *)surface->pixels)[4 * (surface->pitch / 4) + 3];
		if (pixel == 0xFFFF0000u) {
			result = 0;
		} else {
			fprintf(stderr, "unexpected pixel value 0x%08x\n", (unsigned int)pixel);
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroySurface(surface);
	printf("SDL3_gfx %d.%d.%d consumer: %s\n", SDL3_GFXPRIMITIVES_MAJOR, SDL3_GFXPRIMITIVES_MINOR,
		SDL3_GFXPRIMITIVES_MICRO, result == 0 ? "OK" : "FAILED");
	return result;
}
