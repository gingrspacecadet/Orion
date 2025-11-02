#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "machine.h"

void draw_debug(Machine* m, SDL_Renderer* renderer, TTF_Font* font);

#endif