#ifndef CHIP8_VIDEO_H
#define CHIP8_VIDEO_H

#include <stdbool.h>

// Video module.
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

#endif // CHIP8_VIDEO_H
