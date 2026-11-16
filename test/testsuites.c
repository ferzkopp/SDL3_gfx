/*
 * Test driver for the SDL3_gfx automated tests.
 *
 *   gfxtests [--filter <suite|test>] [--seed <seed>] [--golden-dir <dir>] [--data-dir <dir>]
 *            [--output-dir <dir>] [--generate] [--real-video]
 *
 * --generate writes the golden images / checksums from the current code instead of comparing.
 * --real-video uses the platform video driver instead of "dummy" (needed by the PrimitivesGPU suite).
 */
#include "testutils.h"

#include <SDL3/SDL_main.h>

extern SDLTest_TestSuiteReference primitivesTestSuite;
extern SDLTest_TestSuiteReference rotozoomTestSuite;
extern SDLTest_TestSuiteReference imageFilterTestSuite;
extern SDLTest_TestSuiteReference framerateTestSuite;
extern SDLTest_TestSuiteReference framerateTimingTestSuite;
extern SDLTest_TestSuiteReference primitivesGpuTestSuite;

static SDLTest_TestSuiteReference *testSuites[] = {
	&primitivesTestSuite,
	&rotozoomTestSuite,
	&imageFilterTestSuite,
	&framerateTestSuite,
	&framerateTimingTestSuite,
	&primitivesGpuTestSuite,
	NULL};

int main(int argc, char *argv[])
{
	SDLTest_CommonState *state;
	SDLTest_TestSuiteRunner *runner;
	char **args;
	int i, nargs, consumed, result;
	int realVideo = 0;
	int haveSeed = 0;

	/* The runner parses --filter, --seed etc. (SDLTest_CommonArg); a fixed default seed keeps runs reproducible */
	args = (char **)SDL_calloc((size_t)argc + 3, sizeof(char *));
	if (args == NULL) {
		return 1;
	}
	nargs = 0;
	for (i = 0; i < argc; i++) {
		if (SDL_strcmp(argv[i], "--seed") == 0) {
			haveSeed = 1;
		}
		args[nargs++] = argv[i];
	}
	if (!haveSeed) {
		args[nargs++] = "--seed";
		args[nargs++] = "SDL3GFXTEST";
	}

	state = SDLTest_CommonCreateState(args, 0);
	if (state == NULL) {
		SDL_free(args);
		return 1;
	}
	runner = SDLTest_CreateTestSuiteRunner(state, testSuites);

	for (i = 1; i < nargs;) {
		const char *a = args[i];
		const char *v = (i + 1 < nargs) ? args[i + 1] : NULL;
		consumed = 0;
		if (SDL_strcmp(a, "--generate") == 0) {
			gfxtest_options.generate = 1;
			consumed = 1;
		} else if (SDL_strcmp(a, "--real-video") == 0) {
			realVideo = 1;
			consumed = 1;
		} else if (v && SDL_strcmp(a, "--golden-dir") == 0) {
			gfxtest_options.golden_dir = v;
			consumed = 2;
		} else if (v && SDL_strcmp(a, "--data-dir") == 0) {
			gfxtest_options.data_dir = v;
			consumed = 2;
		} else if (v && SDL_strcmp(a, "--output-dir") == 0) {
			gfxtest_options.output_dir = v;
			consumed = 2;
		} else {
			consumed = SDLTest_CommonArg(state, i);
		}
		if (consumed <= 0) {
			static const char *options[] = {"[--golden-dir <dir>]", "[--data-dir <dir>]", "[--output-dir <dir>]", "[--generate]",
				"[--real-video]", NULL};
			SDLTest_CommonLogUsage(state, args[0], options);
			SDLTest_DestroyTestSuiteRunner(runner);
			SDLTest_CommonDestroyState(state);
			SDL_free(args);
			return 2;
		}
		i += consumed;
	}

	/* Headless: the tests use software renderers on surfaces (except PrimitivesGPU). */
	if (!realVideo) {
		SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
	}
	if (!SDL_Init(SDL_INIT_VIDEO) && !realVideo) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		result = 1;
	} else {
		/* Without a display the hardware renderer tests skip themselves */
		result = SDLTest_ExecuteTestSuiteRunner(runner);
	}

	SDLTest_DestroyTestSuiteRunner(runner);
	SDLTest_CommonQuit(state);
	SDL_free(args);
	return result;
}