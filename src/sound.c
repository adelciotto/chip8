#include "SDL.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sound.h"
#include "vm.h"

static SDL_AudioDeviceID audioDevice = 0;
static int audioSampleCount = 0;
static int latencySampleCount = 0;
static void *audioBuffer = NULL;
static int audioBufferLen = 0;

static int toneHz = 256;
static int toneVolume = 3000;
static uint32_t runningSampleIndex = 0;
static int squareWavePeriod;
static int halfSquareWavePeriod;

int SoundInit(int refreshRate)
{
    if (SDL_GetNumAudioDevices(0) <= 0) {
        audioDevice = 0;
        fprintf(stderr, "No audio devices found!\n");
        return 0;
    }

    SDL_AudioSpec want, have;
    SDL_memset(&want, 0, sizeof(want));
    want.freq = 48000;
    want.format = AUDIO_S16LSB;
    want.channels = 1;
    want.samples = want.freq * 1 / refreshRate;

    audioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audioDevice == 0) {
        fprintf(stderr, "Failed to open SDL audio device! %s\n",
                SDL_GetError());
        return -1;
    }

    audioSampleCount = have.samples * have.channels;
    latencySampleCount = have.freq / 15;
    audioBufferLen = latencySampleCount * sizeof(int16_t);
    audioBuffer = malloc(audioBufferLen);
    if (!audioBuffer) {
        fprintf(stderr,
                "Failed to allocate memory for audio buffer! size needed: %d\n",
                audioBufferLen);
        return -1;
    }

    squareWavePeriod = have.freq / toneHz;
    halfSquareWavePeriod = squareWavePeriod / 2;

    SDL_PauseAudioDevice(audioDevice, 0);

    printf(
        "Sound module initialised! freq: %d, latency samples: %d, buffer size: %d\n",
        audioSampleCount, latencySampleCount, audioBufferLen);
    return 0;
}

void SoundDestroy()
{
    if (audioBuffer) {
        free(audioBuffer);
    }
    if (audioDevice > 0) {
        SDL_CloseAudioDevice(audioDevice);
    }
    audioDevice = 0;

    printf("Sound module destroyed!\n");
}

void SoundPlay()
{
    if (audioDevice == 0 || VMGetSoundTimer() <= 0) {
        return;
    }

    int16_t *buffer = (int16_t *)audioBuffer;
    int sampleCount =
        latencySampleCount - SDL_GetQueuedAudioSize(audioDevice) / 2;

    for (int i = 0; i < sampleCount; i++) {
        buffer[i] = ((++runningSampleIndex / halfSquareWavePeriod) % 2) ?
                        toneVolume :
                        -toneVolume;
    }

    SDL_QueueAudio(audioDevice, buffer, sampleCount * 2);
}
