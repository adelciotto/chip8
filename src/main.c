#include "SDL.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "audio.h"
#include "video.h"

#define MAX_SCALE 8

static int optionWinScale = 8;
static bool optionFullscreen = false;
static const char *optionRomPath = "test_opcode.ch8";
static int optionCycles = 20;
static VMColorPaletteType optionPalette = VMCOLOR_PALETTE_NOKIA;

static char *winTitle = NULL;
static int refreshRate = 0;
static bool running = false;

static void AtExit(void);
static void ParseCommandArgs(int argc, char *argv[]);
static void ProcessEvent(SDL_Event *event);

static uint64_t GetPerformanceCounter()
{
	return SDL_GetPerformanceCounter();
}

static double GetElapsedSeconds(uint64_t end, uint64_t start)
{
	return (double)(end - start) / (double)SDL_GetPerformanceFrequency();
}

int main(int argc, char *argv[])
{
	atexit(AtExit);

	ParseCommandArgs(argc, argv);
	printf("Option 'win_scale' set to %d\n", optionWinScale);
	printf("Option 'fullscreen' set to %d\n", optionFullscreen);
	printf("Option 'rom_path' set to %s\n", optionRomPath);
	printf("Option 'cycles' set to %d\n", optionCycles);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "Failed to init SDL! %s\n", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	size_t sz = snprintf(NULL, 0, "CHIP-8 - %s", optionRomPath);
	winTitle = malloc(sz + 1);
	snprintf(winTitle, sz + 1, "CHIP8 - %s", optionRomPath);
	if (VideoInit(winTitle, optionWinScale, optionFullscreen) != 0) {
		fprintf(stderr, "Failed to init video!\n");
		exit(EXIT_FAILURE);
	}
	refreshRate = VideoGetRefreshRate();
	if (refreshRate == 0) {
		refreshRate = 60;
		fprintf(stderr,
				"Failed to get monitor refresh rate! Defaulting to 60hz\n");
	}

	if (AudioInit(refreshRate) != 0) {
		fprintf(stderr, "Failed to init audio!\n");
		exit(EXIT_SUCCESS);
	}

	VMInit(optionCycles, refreshRate, optionPalette);

	if (VMLoadRom(optionRomPath) != 0) {
		fprintf(stderr, "Failed to load CHIP-8 rom %s!\n", optionRomPath);
		exit(EXIT_SUCCESS);
	}

	double targetSecsPerFrame = 1.0 / (double)refreshRate;
	uint64_t lastCounter = 0, metricsCounter = 0;
	uint64_t perfFreq = SDL_GetPerformanceFrequency();

	running = true;
	while (running) {
		// Process all the platform events.
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			ProcessEvent(&event);
		}

		// Update the CHIP-8 VM.
		VMUpdate(targetSecsPerFrame);

		// Push the current audio.
		AudioPush();

		// Cap the FPS to monitor refresh rate.
		if (lastCounter == 0) {
			lastCounter = GetPerformanceCounter();
			metricsCounter = GetPerformanceCounter();
		}
		if (GetElapsedSeconds(GetPerformanceCounter(), lastCounter) <
			targetSecsPerFrame) {
			double elapsed =
				GetElapsedSeconds(SDL_GetPerformanceCounter(), lastCounter);
			int32_t timeToSleep =
				(int32_t)((targetSecsPerFrame - elapsed) * 1000.0) - 1;
			if (timeToSleep > 0) {
				SDL_Delay(timeToSleep);
			}
			while (GetElapsedSeconds(GetPerformanceCounter(), lastCounter) <
				   targetSecsPerFrame) {
				// Waiting...
			}
		}

		uint64_t endCounter = GetPerformanceCounter();

		VideoPresent();

		// TODO: Draw onto screen...
		if (GetElapsedSeconds(GetPerformanceCounter(), metricsCounter) > 1.0) {
			uint64_t counterElapsed = endCounter - lastCounter;
			double msPerFrame =
				(((1000.0 * (double)counterElapsed) / (double)perfFreq));
			double fps = (double)perfFreq / (double)counterElapsed;
			printf("%.02fms/f, %.02f/s\n", msPerFrame, fps);
			metricsCounter = GetPerformanceCounter();
		}

		lastCounter = endCounter;
	}

	return EXIT_SUCCESS;
}

static void AtExit(void)
{
	AudioDestroy();

	VideoDestroy();

	SDL_Quit();

	if (winTitle) {
		free(winTitle);
	}
}

#define IFOPTION(name) if (strcmp((name), argv[i]) == 0)
#define REQUIREARG()                                                           \
	if (++i >= argc) {                                                         \
		hasError = true;                                                       \
		fprintf(stderr, "Option '%s' requires an argument!\n", option);        \
		break;                                                                 \
	}
#define PARSE_UINT_ARG(min, max)                                               \
	REQUIREARG()                                                               \
	int val = atoi(argv[i]);                                                   \
	if (val >= (min) && val <= (max)) {                                        \
		arg = val;                                                             \
	} else {                                                                   \
		hasError = true;                                                       \
		fprintf(                                                               \
			stderr,                                                            \
			"Option '%s' must have int argument in range >= %d && <= %d!\n",   \
			option, (min), (max));                                             \
	}
#define PARSE_BOOL_ARG()                                                            \
	REQUIREARG()                                                                    \
	if (strcmp(argv[i], "true") == 0) {                                             \
		arg = true;                                                                 \
	} else if (strcmp(argv[i], "false") == 0) {                                     \
		arg = false;                                                                \
	} else {                                                                        \
		hasError = true;                                                            \
		fprintf(                                                                    \
			stderr,                                                                 \
			"Option '%s' argument must be a bool argument of 'true' or 'false'!\n", \
			option);                                                                \
	}

static void ParseCommandArgs(int argc, char *argv[])
{
	// No command line args provided.
	// First argument is always the program name.
	if (argc == 1) {
		return 0;
	}

	bool hasError = false;

	for (int i = 1; i < argc; i++) {
		const char *option = argv[i];

		IFOPTION("--win_scale")
		{
			int arg;
			PARSE_UINT_ARG(1, 8)

			if (!hasError) {
				optionWinScale = arg;
			}
		}
		else IFOPTION("--fullscreen")
		{
			bool arg;
			PARSE_BOOL_ARG()

			if (!hasError) {
				optionFullscreen = arg;
			}
		}
		else IFOPTION("--rom")
		{
			REQUIREARG()
			optionRomPath = argv[i];
		}
		else IFOPTION("--cycles")
		{
			int arg;
			PARSE_UINT_ARG(1, 1000)

			if (!hasError) {
				optionCycles = arg;
			}
		}
		else IFOPTION("--palette")
		{
			REQUIREARG()
			IFOPTION("original")
			{
				optionPalette = VMCOLOR_PALETTE_ORIGINAL;
			}
			else IFOPTION("nokia")
			{
				optionPalette = VMCOLOR_PALETTE_NOKIA;
			}
			else IFOPTION("lcd")
			{
				optionPalette = VMCOLOR_PALETTE_LCD;
			}
			else IFOPTION("hotdog")
			{
				optionPalette = VMCOLOR_PALETTE_HOTDOG;
			}
			else IFOPTION("gray")
			{
				optionPalette = VMCOLOR_PALETTE_GRAY;
			}
			else IFOPTION("cga0")
			{
				optionPalette = VMCOLOR_PALETTE_CGA0;
			}
			else IFOPTION("cga1")
			{
				optionPalette = VMCOLOR_PALETTE_CGA1;
			}
			else
			{
				hasError = true;
				fprintf(stderr,
						"--palette option has an unknown value of %s!\n",
						argv[i]);
			}
		}
		else IFOPTION("--help")
		{
			printf(
				"CHIP-8 usage:\n"
				"--rom [filepath:string]        - Load the ROM at 'filepath'. Defaults to opcode_test.ch8\n"
				"--win_scale [scale:uint]       - Scale up the screen in windowed mode. Defaults to 8\n"
				"--fullscreen [fullscreen:bool] - Start in fullscreen mode. Defaults to false\n"
				"--cycles [cycles:uint]         - Cycles to run per frame given 60fps. Defaults to 20\n"
				"						          Value is adjusted for real framerate automatically\n"
				"--palette [palette:string]     - Use the selected color palette. Defaults to 'nokia'\n"
				"--help                         - Print this help text and exit\n");
			exit(EXIT_SUCCESS);
		}
		else
		{
			hasError = true;
			fprintf(stderr, "Option '%s' not recognised\n!", option);
		}
	}

	if (hasError) {
		fprintf(stderr, "Error parsing command options\n!");
	}
}

static bool CheckFullscreenToggle(SDL_Keysym sym)
{
	uint16_t flags = (KMOD_LALT | KMOD_RALT);
#if defined(__MACOSX__)
	flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
	return (sym.scancode == SDL_SCANCODE_RETURN ||
			sym.scancode == SDL_SCANCODE_KP_ENTER) &&
		   (sym.mod & flags) != 0;
}

static bool CheckScreenshot(SDL_Keysym sym)
{
	return (sym.sym == SDLK_PRINTSCREEN);
}

static int MapKey(SDL_Scancode sc)
{
	switch (sc) {
	case SDLK_1:
		return 0x01;
	case SDLK_2:
		return 0x02;
	case SDLK_3:
		return 0x03;
	case SDLK_4:
		return 0x0C;
	case SDLK_q:
		return 0x04;
	case SDLK_w:
		return 0x05;
	case SDLK_e:
		return 0x06;
	case SDLK_r:
		return 0x0D;
	case SDLK_a:
		return 0x07;
	case SDLK_s:
		return 0x08;
	case SDLK_d:
		return 0x09;
	case SDLK_f:
		return 0x0E;
	case SDLK_z:
		return 0x0A;
	case SDLK_x:
		return 0x00;
	case SDLK_c:
		return 0x0B;
	case SDLK_v:
		return 0x0F;
	default:
		return -1;
	}
}

static void ProcessEvent(SDL_Event *event)
{
	switch (event->type) {
	case SDL_QUIT:
		running = false;
		break;
	case SDL_WINDOWEVENT: {
		switch (event->window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			VMTogglePause(false);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			VMTogglePause(true);
			break;
		}
	} break;
	case SDL_KEYDOWN: {
		if (CheckFullscreenToggle(event->key.keysym)) {
			VMTogglePause(true);
			VideoToggleFullscreen();
			VMTogglePause(false);
			return;
		}

		if (CheckScreenshot(event->key.keysym)) {
			VideoScreenshot();
			return;
		}

		int key = MapKey(event->key.keysym.sym);
		if (key >= 0) {
			VMSetKey(key);
		}
	} break;
	case SDL_KEYUP: {
		int key = MapKey(event->key.keysym.sym);
		if (key >= 0) {
			VMClearKey(key);
		}
	} break;
	}
}
