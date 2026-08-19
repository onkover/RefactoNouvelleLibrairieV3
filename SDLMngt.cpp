#define SDLMNGT

#include "pch.h"          // première ligne, toujours
#include "main.h"
#include <iostream>		// pour std::cout, std::cin

using namespace std;

#ifndef VSYNC
	#define VSYNC
#endif

void SDLkill(void)
{
	if (myFont != NULL)
	{
		TTF_CloseFont(myFont);
		TTF_Quit();
	}

	if (SDLtexture != NULL) SDL_DestroyTexture(SDLtexture);
	if (surfaceWds != NULL) SDL_FreeSurface(surfaceWds);
	if (SDLrenderer != NULL) SDL_DestroyRenderer(SDLrenderer);
	if (window != NULL) SDL_DestroyWindow(window);

}


bool SDLINIT(int ScreenWidth, int ScreenHeight)
{
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
	{
		std::cout << "SDL initialization failed. SDL Error: " << SDL_GetError();
		SDLkill();
		std::cin.get();
		return false;
	}

	// Font
	if (TTF_Init() == -1)
	{
		std::cout << "SDL_TTF initialization failed. SDL Error: " << SDL_GetError();
		SDLkill();
		std::cin.get();
		return false;
	}
	else
	{
		myFont = TTF_OpenFont("Tahoma.ttf", 20);
		if (myFont == NULL)
		{
			std::cout << "Could not load font: " << TTF_GetError();
			SDLkill();
			std::cin.get();
			return false;
		}

	}


	// Create an application window
	window = SDL_CreateWindow(
		"Onk's SDL2 window",                  // window title
		SDL_WINDOWPOS_UNDEFINED,           // initial x position
		SDL_WINDOWPOS_UNDEFINED,           // initial y position
		ScreenWidth,                               // width, in pixels
		ScreenHeight,                               // height, in pixels
		SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);   // flags 

	if (window == NULL) {
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		SDLkill();
		std::cin.get();
		return false;
	}


#ifdef VSYNC
	// Bizarrement, sans SDL_RENDERER_PRESENTVSYNC, ca à l'air aussi synchronisé
	SDLrenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);	// | SDL_RENDERER_PRESENTVSYNC, SDL_RENDERER_SOFTWARE
	if (SDLrenderer == NULL) {
		fprintf(stderr, "\nRenderer could not be created:%s\n", SDL_GetError());
		SDLkill();
		std::cin.get();
		return false;
	}

	SDLtexture = SDL_CreateTexture(SDLrenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ScreenWidth, ScreenHeight);	//SDL_TEXTUREACCESS_TARGET
	if (SDLtexture == NULL) {
		fprintf(stderr, "\nTexture could not be created:%s\n", SDL_GetError());
		SDLkill();
		std::cin.get();
		return false;
	}

#else

	//	surfaceSRC = SDL_CreateRGBSurface(0, SCREEN_HIGH, SCREEN_WIDTH, 24, 0xff000000, 0x00ff0000, 0x0000ff00, 0);
	surfaceWds = SDL_GetWindowSurface(window);
	if (surfaceWds == NULL) {
		fprintf(stderr, "\nSurface could not be created:%s\n", SDL_GetError());
		SDLkill();
		cin.get();
		return FALSE;
	}
#endif

	SDL_GetWindowPosition(window, &xPosWindow, &yPosWindow);
	XposMouseDefault = xPosWindow + ScreenWidth;
	YposMouseDefault = yPosWindow + ScreenHeight;
	SDL_WarpMouseInWindow(window, XposMouseDefault, YposMouseDefault);

#ifdef VSYNC
	//	myscreen = nullptr;	// sera positionné avec SDL_LockTexture
	//myScreen.setScreen(nullptr, ScreenWidth, ScreenHeight);
#else
	//	myscreen = (int*)surfaceWds->pixels;	// Positionné avec SDL_Surface
	myScreen.setScreen((int*)surfaceWds->pixels, ScreenWidth, ScreenHeight);
#endif

	return true;
}







