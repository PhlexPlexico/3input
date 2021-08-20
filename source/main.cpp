
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#include "servertypes.h"
#include "ui.h"
#include "irrst.hpp"
/*
    3Input - Input Reader background process for the 3DS.
    Copyright (C) 2020 
	PhlexPlexico (https://github.com/phlexplexico)
	n3rdswithgame (https://github.com/n3rdswithgame)


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.


    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** From hid.c, used to check if new 3ds is in use or not.*/
bool __attribute__((weak)) hidShouldUseIrrst(void)
{
	bool val;
	return R_SUCCEEDED(APT_CheckNew3DS(&val)) && val;
}

void initScreens(PrintConsole *windowOne, PrintConsole *windowTwo, PrintConsole *windowThree, PrintConsole *bottomWindow)
{
	// Initialize services
	gfxInitDefault();

	//In this example we need three PrintConsole, one for each window and one for the whole top screen
	//PrintConsole leftWindow, rightWindow, topScreen;

	//Initialize console for each window
	consoleInit(GFX_TOP, windowOne);
	consoleInit(GFX_TOP, windowTwo);
	consoleInit(GFX_TOP, windowThree);
	consoleInit(GFX_BOTTOM, bottomWindow);
	//Now we specify the window position and dimension for each console window using consoleSetWindow(PrintConsole* console, int x, int y, int width, int height);
	//x, y, width and height are in terms of cells, not pixel, where each cell is composed by 8x8 pixels.
	consoleSetWindow(windowOne, 1, 1, 23, 28);
	consoleSetWindow(windowTwo, 26, 1, 23, 28);
	//Before doing any text printing we should select the PrintConsole in which we are willing to write, otherwise the library will print on the last selected/initialized one
	//Let's start by printing something on the top screen
}

int brew_launch(int argc, char **argv)
{
	server_t serv;
	aptSetSleepAllowed(false);
	make_input_server(&serv);

	PrintConsole leftWindow, rightWindow, topScreen, bottomScreen;
	initScreens(&leftWindow, &rightWindow, &topScreen, &bottomScreen);
	consoleSelect(&leftWindow);
	//printf("This text is in the \nleft window!\n");
	printf("Hold SEL and press \n[ABXY] to change freq\n");
	//Now write something else on the bottom screen
	consoleSelect(&rightWindow);
	printf("[B]   1 times per sec\n");
	printf("[A]   5 times per sec\n");
	printf("[X]  10 times per sec\n");
	printf("[Y]  80 times per sec\n");
	consoleSelect(&topScreen);
	printf("\x1b[28;0H--------------------------------------------------");
	printf("\x1b[30;14HPress START & SEL to exit \x2");
	consoleSelect(&bottomScreen);

	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame

		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kHold = hidKeysHeld();
		u32 kPress = hidKeysDown();
		if (kPress & KEY_CPAD_DOWN)
		{
			consoleClear();
			printf("\x1b[4;1HPRESSED CPAD DOWN!");
		}
		if (kHold & KEY_SELECT)
		{
			if (kPress & KEY_B)
				server_change_timer_freq(&serv, 1, 1, &bottomScreen);
			if (kPress & KEY_A)
				server_change_timer_freq(&serv, 1, 5, &bottomScreen);
			if (kPress & KEY_Y)
				server_change_timer_freq(&serv, 1, 10, &bottomScreen);
			if (kPress & KEY_X)
				server_change_timer_freq(&serv, 1, 80, &bottomScreen);
			if (kPress & KEY_START)
				break;
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		//Wait for VBlank
		gspWaitForVBlank();
	}
	// Exit services
	// Flush and swap framebuffers
	server_dtor(&serv);
	amExit();
	aptExit();
	ptmSysmExit();
	gfxExit();
	return 0;
}

int main_daemon(int argc, char **argv)
{
	server_t serv;
	int freqStep;

	make_input_server(&serv);
	freqStep = 10;
	if (!R_SUCCEEDED(iruInit_()))
	{
		server_dtor(&serv);
		return -1;
	}

	while (1)
	{
		hidScanInput();
		iruScanInput_();
		irSampling();
		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kHold = iruKeysHeld_() | hidKeysDown();
		if (kHold & KEY_SELECT)
		{
			if (kHold & KEY_B)
				server_change_timer_freq(&serv, 1, 1, NULL);
			if (kHold & KEY_A)
				server_change_timer_freq(&serv, 1, 10, NULL);
			if (kHold & KEY_Y)
				server_change_timer_freq(&serv, 1, 60, NULL);
			if (kHold & KEY_X)
				server_change_timer_freq(&serv, 1, 140, NULL);
			if (kHold & KEY_L)
			{
				freqStep = freqStep - 10;
				if (freqStep - 10 < 0)
					freqStep = 0;
				server_change_timer_freq(&serv, 1, freqStep, NULL);
			}
			if (kHold & KEY_R)
			{
				freqStep = freqStep + 10;
				server_change_timer_freq(&serv, 1, freqStep, NULL);
			}
			if (kHold & (KEY_START | KEY_START))
				break;
		}
		svcSleepThread(1e+9);
	}
	server_dtor(&serv);
	return 0;
}

int main(int argc, char **argv)
{

	int is_daemon = 1; // will figure this out later
	if (is_daemon)
	{
		return main_daemon(argc, argv);
		;
	}
	else
	{
		return brew_launch(argc, argv);
	}
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...)
{
	//---------------------------------------------------------------------------------

	ERRF_ThrowResultWithMessage(-1, fmt);
	exit(0);
}
