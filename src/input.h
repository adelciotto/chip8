#ifndef CHIP8_INPUT_H
#define CHIP8_INPUT_H

#include "def.h"

// Input module.
// Provides keyboard input and event processing using SDL.

// InputPollEvents() - Polls and responds to events.
void InputPollEvents();

// InputCloseRequested() - Returns true if the user has requested to close
// the application.
bool InputCloseRequested();

#endif //CHIP8_INPUT_H
