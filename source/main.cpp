
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


int main_daemon(int argc, char **argv)
{
	server_t serv;
	int freqStep;
	freqStep = 10;
	make_input_server(&serv, freqStep);
	
	if (!R_SUCCEEDED(iruInit_()))
	{
		server_dtor(&serv);
		return -1;
	}
	if(hidShouldUseIrrst())
		irrstExit();

	while (1)
	{
		irSampling();
		hidScanInput();
		iruScanInput_();
		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kHold = iruKeysHeld_() | hidKeysDown();
		if (kHold & (KEY_B | KEY_SELECT))
			server_change_timer_freq(&serv, 1, 1, NULL);
		if (kHold & (KEY_A | KEY_SELECT))
			server_change_timer_freq(&serv, 1, 10, NULL);
		if (kHold & (KEY_Y | KEY_SELECT))
			server_change_timer_freq(&serv, 1, 60, NULL);
		if (kHold & (KEY_X | KEY_SELECT))
			server_change_timer_freq(&serv, 1, 140, NULL);
		if (kHold & (KEY_L | KEY_SELECT)) {
				freqStep = freqStep - 10;
				if (freqStep - 10 < 0)
					freqStep = 0;
				server_change_timer_freq(&serv, 1, freqStep, NULL);
			}
		if (kHold & (KEY_R | KEY_SELECT)) { 
				freqStep = freqStep + 10;
				server_change_timer_freq(&serv, 1, freqStep, NULL);
			}
		if (kHold & (KEY_START | KEY_SELECT))
			break;
		svcSleepThread(1e+9);
	}
	server_dtor(&serv);
	return 0;
}

int main(int argc, char **argv)
{
	return main_daemon(argc, argv);
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...)
{
	//---------------------------------------------------------------------------------

	ERRF_ThrowResultWithMessage(-1, fmt);
	exit(0);
}
