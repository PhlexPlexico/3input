#include <3ds.h>

#include <errno.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "ir_patch.h"

#define INPUT_TID 0x0004013000495602ULL

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;
s32 sock = -1;

void getIpAddr(){
    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
    if(SOC_buffer == NULL) {
        printf("memalign: failed to allocate\n");
    }
    socInit(SOC_buffer, SOC_BUFFERSIZE);
    struct sockaddr_in server;
    sock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
    memset (&server, 0, sizeof (server));
    server.sin_family = AF_INET;
    server.sin_port = htons (80);
    server.sin_addr.s_addr = gethostid();
    //Move the cursor to the middle of the screen
    printf("\x1b[15;9H");
    printf("\x1b[47;30mIP Address: %s:65534\x1b[0m", inet_ntoa(server.sin_addr));
    printf("\x1b[16;8HUse this IP in your input viewer!");
    close(sock);
    socExit();
}


/**
 * ZL/ZR/C-Stick need a patched IR module, which matters on a New 3DS or an
 * Old 3DS with a Circle Pad Pro attached. Check for (and install) the patch
 * there, and say so on the top screen.
 */
void showIrPatchStatus(){
    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);

    printf("\x1b[1;1H");
    if(!isNew3DS){
        printf("Checking for a Circle Pad Pro...");
        gfxFlushBuffers();
        CppState cpp = cppDetect();
        printf("\x1b[1;1H\x1b[2K");
        if(cpp == CPP_ABSENT){
            printf("IR patch: not needed (no New 3DS or Circle Pad Pro).");
            return;
        }
        if(cpp == CPP_UNKNOWN){
            printf("IR patch: could not check for a Circle Pad Pro,\nskipping.");
            return;
        }
    }

    int err = 0;
    switch(irPatchEnsureInstalled(&err)){
        case PATCH_PRESENT:
            printf("\x1b[32mIR patch: installed.\x1b[0m");
            break;
        case PATCH_PRESENT_DIFFERENT:
            printf("\x1b[32mIR patch: installed.\x1b[0m (differs from the\nbundled copy, left untouched)");
            break;
        case PATCH_INSTALLED:
            printf("\x1b[33mIR patch was missing and has been installed to\n%s\n\nRESTART YOUR CONSOLE, then relaunch this app.\nIn Luma's config, turn on 'Enable loading\nexternal FIRMs and modules'.\x1b[0m", IR_PATCH_PATH);
            break;
        default:
            printf("\x1b[31mIR patch: could not install it (%s).\nCopy code.ips to\n%s\x1b[0m", strerror(err), IR_PATCH_PATH);
            break;
    }
}

int main(int argc, char** argv) {
    u32 isConnected;
    u32 pid;
    u32 kDown = 0;
    PrintConsole topScreen, bottomScreen;
    nsInit();
    hidInit();
    acInit();
    ACU_GetStatus(&isConnected);
    gfxInitDefault();
	//Initialize console for both screen using the two different PrintConsole we have defined
	consoleInit(GFX_TOP, &topScreen);
    consoleInit(GFX_BOTTOM, &bottomScreen);

    consoleSelect(&topScreen);
    showIrPatchStatus();
    if(isConnected == 3){
        getIpAddr();
        printf("\x1b[30;7HPress A to launch 3Input, B to exit\x1");
        consoleSelect(&bottomScreen);
    } else{
        printf("\x1b[1;1H");
        printf("\x1b[31mNot connected to the internet.\n\x1b[0m");
        printf("\x1b[31m3Input will not launch. Please connect to the\n\x1b[0m");
        printf("\x1b[31minternet and try again.\x1b[0m");
        printf("\x1b[30;20HPress B to exit");
    }
    // Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		kDown = hidKeysDown();

		if ((kDown & KEY_A) & (isConnected == 3)) break; // break in order to return to hbmenu
        if (kDown & KEY_B) break; 
		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();

		//Wait for VBlank
		gspWaitForVBlank();
	}
    if(kDown == KEY_A) {
        NS_TerminateProcessTID(INPUT_TID, 10e+9);
        NS_LaunchTitle(INPUT_TID, 0, &pid);
    }
    gfxExit();
    acExit();
    hidExit();
    nsExit();
    
    return 0;
}