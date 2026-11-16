/*
 * Replay driver: runs a fuzz target on input files instead of under libFuzzer. CTest uses it to
 * replay the committed corpus (fuzz/corpus/<target>/) in every build; it also reproduces crashes:
 *
 *   fuzz_primitives_replay crash-1234abcd
 */
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdio.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int main(int argc, char *argv[])
{
	int i, errors = 0;
	if (argc < 2) {
		fprintf(stderr, "usage: %s <input-file>...\n", argv[0]);
		return 2;
	}
	for (i = 1; i < argc; i++) {
		size_t size = 0;
		void *data = SDL_LoadFile(argv[i], &size);
		if (data == NULL) {
			fprintf(stderr, "cannot read %s: %s\n", argv[i], SDL_GetError());
			errors++;
			continue;
		}
		LLVMFuzzerTestOneInput((const uint8_t *)data, size);
		SDL_free(data);
	}
	printf("Replayed %d input(s), %d read error(s)\n", argc - 1 - errors, errors);
	return errors ? 1 : 0;
}
