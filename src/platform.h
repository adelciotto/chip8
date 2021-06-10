#ifndef CHIP8_PLATFORM_H
#define CHIP8_PLATFORM_H

#include "def.h"

// Video functions.
// Presents the CHIP8 display to the screen using SDL.

// VideoInit() - Initialises the Video module with the given parameters.
int VideoInit(const char *winTitle, int winScale, bool fullscreen);

// VideoDestroy() - Destroys the Video module and all associated resources.
void VideoDestroy();

// VideoPresent() - Uploads display pixels to screen and waits for Vsync.
void VideoPresent();

// VideoGetRefreshRate() - Get the refresh rate of the primary monitor.
int VideoGetRefreshRate();

// VideoToggleFullscreen() - Toggles fullscreen mode.
int VideoToggleFullscreen();

// VideoScreenshot() - Takes a screenshot and saves the image file to the
// same directory as the executable.
void VideoScreenshot();

// VideoSetWindowTitle() - Sets the title of the window.
void VideoSetWindowTitle(const char *fmt, ...);

// Sound functions.
// Plays sound using SDL.

// SoundInit() - Initialises the Sound module.
int SoundInit();

// SoundDestroy() - Destroys the Sound module and all associated resources.
void SoundDestroy();

// SoundPlay() - Plays a beep sound if the VM is requesting sound.
void SoundPlay();

// Input functions.
// Provides keyboard input and event processing using SDL.

// InputPollEvents() - Polls and responds to events.
void InputPollEvents();

// InputCloseRequested() - Returns true if the user has requested to close
// the application.
bool InputCloseRequested();

#endif //CHIP8_PLATFORM_H
