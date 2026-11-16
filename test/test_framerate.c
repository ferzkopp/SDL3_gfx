/*
 * Tests for SDL3_framerate: parameter validation and rate bookkeeping ("Framerate" suite) and
 * timing accuracy ("FramerateTiming" suite, labeled 'timing' in CTest and excluded by default
 * because it depends on the machine load).
 */
#include "testutils.h"

#include "SDL3_framerate.h"

static int test_api(void *arg)
{
	FPSmanager m;
	(void)arg;
	SDL_initFramerate(&m);
	SDLTest_AssertCheck(SDL_getFramerate(&m) == FPS_DEFAULT, "Default rate is FPS_DEFAULT (%d)", SDL_getFramerate(&m));
	SDLTest_AssertCheck(SDL_getFramecount(&m) == 0, "Initial frame count is 0");
	SDLTest_AssertCheck(SDL_setFramerate(&m, 60) == 0 && SDL_getFramerate(&m) == 60, "setFramerate(60)");
	SDLTest_AssertCheck(SDL_setFramerate(&m, FPS_LOWER_LIMIT) == 0, "setFramerate(FPS_LOWER_LIMIT) accepted");
	SDLTest_AssertCheck(SDL_setFramerate(&m, FPS_UPPER_LIMIT) == 0, "setFramerate(FPS_UPPER_LIMIT) accepted");
	SDLTest_AssertCheck(SDL_setFramerate(&m, FPS_LOWER_LIMIT - 1) == -1, "setFramerate below the lower limit returns -1");
	SDLTest_AssertCheck(SDL_setFramerate(&m, FPS_UPPER_LIMIT + 1) == -1, "setFramerate above the upper limit returns -1");
	SDLTest_AssertCheck(SDL_getFramerate(&m) == FPS_UPPER_LIMIT, "Rejected rates do not change the rate");
	SDLTest_AssertCheck(SDL_getFramerate(NULL) == -1, "getFramerate(NULL) returns -1");
	SDLTest_AssertCheck(SDL_getFramecount(NULL) == -1, "getFramecount(NULL) returns -1");
	SDLTest_AssertCheck(SDL_framerateDelay(NULL) == 0, "framerateDelay(NULL) returns 0");
	return TEST_COMPLETED;
}

/* FRM-001: SDL_setFramerate dereferenced a NULL manager. */
static int test_null_manager(void *arg)
{
	(void)arg;
	SDLTest_AssertCheck(SDL_setFramerate(NULL, 30) == -1, "setFramerate(NULL) returns -1");
	SDL_initFramerate(NULL);
	SDLTest_AssertPass("initFramerate(NULL) returns");
	return TEST_COMPLETED;
}

static int test_framecount(void *arg)
{
	FPSmanager m;
	int i;
	(void)arg;
	SDL_initFramerate(&m);
	SDL_setFramerate(&m, FPS_UPPER_LIMIT);
	for (i = 0; i < 5; i++) {
		SDL_framerateDelay(&m);
	}
	SDLTest_AssertCheck(SDL_getFramecount(&m) == 5, "Frame count after 5 delays is %d", SDL_getFramecount(&m));
	SDL_setFramerate(&m, 50);
	SDLTest_AssertCheck(SDL_getFramecount(&m) == 0, "setFramerate resets the frame count");
	return TEST_COMPLETED;
}

/* 20 frames at 50 Hz must take about 400 ms (the manager compensates for delay jitter). */
static int test_timing(void *arg)
{
	FPSmanager m;
	Uint64 start, elapsed;
	int i;
	(void)arg;
	SDL_initFramerate(&m);
	SDL_setFramerate(&m, 50);
	SDL_framerateDelay(&m);
	start = SDL_GetTicks();
	for (i = 0; i < 20; i++) {
		SDL_framerateDelay(&m);
	}
	elapsed = SDL_GetTicks() - start;
	SDLTest_AssertCheck(elapsed >= 380 && elapsed <= 460, "20 frames at 50 Hz took %u ms (expected ~400)", (unsigned int)elapsed);
	return TEST_COMPLETED;
}

static SDL_AtomicInt delay_done;

static int SDLCALL delay_thread(void *data)
{
	SDL_framerateDelay((FPSmanager *)data);
	SDL_SetAtomicInt(&delay_done, 1);
	return 0;
}

/* A target time in the past must not cause a delay, also when the tick values wrap around
   (FRM-002: the unsigned comparison turned "3 days late" into a delay of ~46 days with 32-bit ticks). */
static int test_tick_wrap(void *arg)
{
	static FPSmanager manager;
	SDL_Thread *thread;
	Uint64 start;
	(void)arg;
	SDL_initFramerate(&manager);
	SDL_setFramerate(&manager, 30);
	/* 3.1 days in the past; wraps to a large value while SDL_GetTicks() is still small */
	manager.baseticks = SDL_GetTicks() - 0x10000000u;
	SDL_SetAtomicInt(&delay_done, 0);
	thread = SDL_CreateThread(delay_thread, "framerateDelay", &manager);
	start = SDL_GetTicks();
	while (!SDL_GetAtomicInt(&delay_done) && SDL_GetTicks() - start < 2000) {
		SDL_Delay(10);
	}
	SDLTest_AssertCheck(SDL_GetAtomicInt(&delay_done), "SDL_framerateDelay returns without delay when behind schedule across a tick wrap");
	if (SDL_GetAtomicInt(&delay_done)) {
		SDL_WaitThread(thread, NULL);
	} else {
		SDL_DetachThread(thread);
	}
	return TEST_COMPLETED;
}

/* Many frames on schedule must keep the target exact (FRM-002: float precision of framecount * rateticks). */
static int test_long_run_precision(void *arg)
{
	FPSmanager manager;
	Uint64 before, after;
	(void)arg;
	SDL_initFramerate(&manager);
	SDL_setFramerate(&manager, 30);
	/* pretend 2^25 frames (12.9 days at 30 Hz) were on schedule: target = now + ~33 ms */
	manager.framecount = 33554432u - 1u;
	manager.baseticks = SDL_GetTicks() - (Uint64)(33554432.0 * (double)manager.rateticks) + 33u;
	before = SDL_GetTicks();
	SDL_framerateDelay(&manager);
	after = SDL_GetTicks();
	SDLTest_AssertCheck(after - before >= 15 && after - before <= 60, "Delay after 2^25 frames is about one frame (%u ms)", (unsigned int)(after - before));
	return TEST_COMPLETED;
}
static const SDLTest_TestCaseReference apiRef = {test_api, "framerate_api", "Parameter validation and limits", TEST_ENABLED};
static const SDLTest_TestCaseReference nullRef = {test_null_manager, "framerate_null_manager", "NULL manager (FRM-001)", TEST_ENABLED};
static const SDLTest_TestCaseReference countRef = {test_framecount, "framerate_framecount", "Frame counting", TEST_ENABLED};
static const SDLTest_TestCaseReference timingRef = {test_timing, "framerate_timing", "Delay accuracy", TEST_ENABLED};

static const SDLTest_TestCaseReference wrapRef = {test_tick_wrap, "framerate_tick_wrap", "Behind schedule across a tick wrap (FRM-002)", TEST_ENABLED};
static const SDLTest_TestCaseReference precisionRef = {test_long_run_precision, "framerate_long_run", "Target time after many frames (FRM-002)", TEST_ENABLED};

static const SDLTest_TestCaseReference *framerateTests[] = {&apiRef, &nullRef, &countRef, &wrapRef, &precisionRef, NULL};
static const SDLTest_TestCaseReference *framerateTimingTests[] = {&timingRef, NULL};

SDLTest_TestSuiteReference framerateTestSuite = {"Framerate", NULL, framerateTests, NULL};
SDLTest_TestSuiteReference framerateTimingTestSuite = {"FramerateTiming", NULL, framerateTimingTests, NULL};
