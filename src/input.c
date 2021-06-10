#include "SDL.h"

#include "platform.h"
#include "vm.h"

static bool closeRequested = false;

static bool CheckFullscreenToggle(SDL_Keysym sym);
static bool CheckScreenshot(SDL_Keysym sym);
static int MapKey(SDL_Scancode sc);

void InputPollEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            closeRequested = true;
            break;
        case SDL_WINDOWEVENT: {
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                VMTogglePause(false);
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                VMTogglePause(true);
                break;
            }
        } break;
        case SDL_KEYDOWN: {
            if (CheckFullscreenToggle(event.key.keysym)) {
                VMTogglePause(true);
                VideoToggleFullscreen();
                VMTogglePause(false);
                return;
            }

            if (CheckScreenshot(event.key.keysym)) {
                VMTogglePause(true);
                VideoScreenshot();
                VMTogglePause(false);
                return;
            }

            int key = MapKey(event.key.keysym.sym);
            if (key >= 0) {
                VMSetKey(key);
            }
        } break;
        case SDL_KEYUP: {
            int key = MapKey(event.key.keysym.sym);
            if (key >= 0) {
                VMClearKey(key);
            }
        } break;
        }
    }
}

bool InputCloseRequested()
{
    return closeRequested;
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
