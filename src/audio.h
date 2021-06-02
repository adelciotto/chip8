#ifndef CHIP8_AUDIO_H
#define CHIP8_AUDIO_H

int AudioInit(int refreshRate);

void AudioDestroy();

void AudioPush();

#endif // CHIP8_AUDIO_H
