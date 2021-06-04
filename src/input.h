#ifndef CHIP8_INPUT_H
#define CHIP8_INPUT_H

#include <stdbool.h>

// Input module.
// Provides keyboard input and event processing using SDL.

// InputInit() - Initialises the Input module.
void InputInit();

// InputPollEvents() - Polls and responds to events.
void InputPollEvents();

// InputCloseRequested() - Returns true if the user has requested to close
// the application.
bool InputCloseRequested();

#endif //CHIP8_INPUT_H
