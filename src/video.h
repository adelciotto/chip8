#ifndef CHIP8_VIDEO_H
#define CHIP8_VIDEO_H

#include <stdbool.h>

int VideoInit(const char *winTitle, int winScale, bool fullscreen);

void VideoDestroy();

void VideoPresent();

int VideoGetRefreshRate();

int VideoToggleFullscreen();

void VideoScreenshot();

#endif // CHIP8_VIDEO_H
