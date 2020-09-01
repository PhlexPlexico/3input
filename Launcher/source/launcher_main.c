#include <3ds.h>

#include <stdio.h>

#define INPUT_TID 0x0004013000495602ULL

#define log(x)  printf("[+]" x "\n");\
                svcSleepThread(5e+8);

int main(int argc, char** argv) {
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, 0);

    nsInit();
    log("nsInit()");

    NS_TerminateProcessTID(INPUT_TID, 10e+9);
    log("NS_TerminateProcessTID");

    u32 pid;
    int ret = NS_LaunchTitle(INPUT_TID, 0, &pid);
    log("NS_LaunchTitle");
    if(ret < 0) {
        printf("Launch failed\n");
        printf("error: 0x%08x", ret);
        printf("%s", osStrError(ret));
        while(aptMainLoop()) {
            hidScanInput();
            if(hidKeysHeld() & KEY_START);
        }
    }
    nsExit();
    log("nsExit()");

    gfxExit();
    u8 param[0x300];
    u8 hmac[0x20];

    memset(param, 0, sizeof(param));
    memset(hmac, 0, sizeof(hmac));
    APT_PrepareToDoApplicationJump(0, 0x0004000000033500LL, 0);
    APT_DoApplicationJump(param, sizeof(param), hmac);

    return 0;
}