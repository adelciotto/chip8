#ifndef CHIP8_SOUND_H
#define CHIP8_SOUND_H

// Implementation Sound module.
// Plays sound using SDL.

// SoundInit() - Initialises the Sound module.
// The refreshRate will determine how many samples are queued to the audio.
// device every frame.
int SoundInit(int refreshRate);

// SoundDestroy() - Destroys the Sound module and all associated resources.
void SoundDestroy();

// SoundPlay() - Plays a beep sound if the VM is requesting sound.
void SoundPlay();

#endif // CHIP8_SOUND_H
