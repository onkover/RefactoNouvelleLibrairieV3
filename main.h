#pragma once
#pragma warning(disable : 4996)

#define SDL_MAIN_HANDLED      // AVANT l'include : neutralise la macro
#include <SDL.h>
#include <SDL_ttf.h>
#include "config.h"


#ifdef MAIN
SDL_Window* window;
SDL_Renderer* SDLrenderer;
SDL_Texture* SDLtexture;
SDL_Surface* surfaceWds;
SDL_Color palette[255];
SDL_Palette* mypalette;

TTF_Font* myFont;

// Image
unsigned int ColorPalette[255];
unsigned char* bufferPic;

int pitch;
//int screenWidth,screenHeight;
int xPosWindow, yPosWindow;
int XposMouseDefault, YposMouseDefault;
int* ptrScreen;


#else
extern SDL_Window* window;
extern SDL_Renderer* SDLrenderer;
extern SDL_Texture* SDLtexture;
extern SDL_Surface* surfaceWds;
extern SDL_Color palette[255];
extern SDL_Palette* mypalette;
extern TTF_Font* myFont;

// Image
extern unsigned int ColorPalette[255];
extern unsigned char* bufferPic;

extern int pitch;
//extern int screenWidth, screenHeight;
extern int xPosWindow, yPosWindow;
extern int XposMouseDefault, YposMouseDefault;
extern int* ptrScreen;
#endif

void SDLkill();
bool SDLINIT(int ScreenWidth, int ScreenHeight);


//extern "C" void CleanScreenV3(void* p, unsigned long long bytes);
