//
//  sdl_server.c
//  CGBA
//
//  Created by Aaron Ruel on 11/4/17.
//  Copyright (c) 2017 AAR. All rights reserved.
//

#include "sdl_server.h"
#include <SDL2/SDL.h>
#include <SDL2_ttf/SDL_ttf.h>

// GBA screen ratio 3:2
#define VP_WIDTH 536
#define VP_HEIGHT (Uint32)((2.f/3.f) * (float)VP_WIDTH)

#define true 1
#define false 0
typedef unsigned int bool;

#define GBA_V_WIDTH   240
#define GBA_V_HEIGHT  160
#define GBA_V_FWIDTH  240.f
#define GBA_V_FHEIGHT 160.f

#define GBA_C_WHITE 0x7FFF

// GBA 15-bit color scheme
// BBBBBGGGGGRRRRR
#define COLOR_MAP 255.f/31.f

// GBA pixel array 240x160
void gba_plot_pixel(SDL_Renderer *renderer, Uint32 x, Uint32 y, Uint16 color) {
    const Uint8 red =   ((color >> 0 ) & 0b11111)* COLOR_MAP;
    const Uint8 green = ((color >> 5 ) & 0b11111)* COLOR_MAP;
    const Uint8 blue =  ((color >> 10) & 0b11111)* COLOR_MAP;
    const float w_ratio = ((float)VP_WIDTH / GBA_V_FWIDTH);
    const float h_ratio = ((float)VP_HEIGHT / GBA_V_FHEIGHT);
    const Uint32 adjx = h_ratio*x;
    const Uint32 adjy = w_ratio*y;
    SDL_Rect r = {adjx,adjy,h_ratio,h_ratio};
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    SDL_RenderFillRect(renderer, &r);
}

void gba_screen_boilerplate(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

void gba_fps_render(SDL_Renderer *renderer, Uint32 proctimestart, SDL_Surface *m_surface, TTF_Font *sans) {
    SDL_Color white = {255,255,255,255};
    const Uint32 proctime = SDL_GetTicks() - proctimestart;
    float delay = 16.666 - proctime;
    if (delay < 0) {
        delay = 0;
    }
    SDL_Delay(delay);
    float fps = 1000/delay;
    char displaystring[50];
    snprintf(displaystring, 50, "%f FPS", fps);
    m_surface = TTF_RenderText_Solid(sans, displaystring, white);
    SDL_Texture *m = SDL_CreateTextureFromSurface(renderer, m_surface);
    SDL_Rect m_r = {0,0,100,20};
    SDL_RenderCopy(renderer, m, NULL, &m_r);
    SDL_FreeSurface(m_surface);
    SDL_DestroyTexture(m);
}

void gba_srv_initialize() {
    // Init var decl
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Init();
    TTF_Font *sans = TTF_OpenFont("/Library/Fonts/Osaka.ttf", 24);
    if (!sans) {
        printf("TTF_OpenFont: %s\n", TTF_GetError());
        return;
    }
    SDL_Surface *fps_surface;
    
    // Init vars
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
                              "GBA",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              VP_WIDTH,
                              VP_HEIGHT,
                              0
                              );
    
    printf("%i, %i", VP_HEIGHT, VP_WIDTH);
    
    renderer = SDL_CreateRenderer(window, 0, 0);
    
    if (window == NULL) {
        printf("SDL Renderer not created");
        return;
    }
    
    // main loop
    bool done = false;
    while (!done) {
        const Uint32 proctimestart = SDL_GetTicks();
        
        /* Render VBlank */
        gba_screen_boilerplate(renderer);
        
        /* Render space */
        
        
        /* FPS overlay */
        gba_fps_render(renderer, proctimestart, fps_surface, sans);
        
        SDL_RenderPresent(renderer);
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    done = true;
                    break;
                default:
                    break;
            }
        }
    }
    
    free(sans);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    
    SDL_Quit();
}