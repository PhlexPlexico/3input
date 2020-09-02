#include <3ds.h>

#include <stdio.h>

#define INPUT_TID 0x0004013000495602ULL

#define log(x)  printf("[+]" x "\n");\
                svcSleepThread(5e+8);

int main(int argc, char** argv) {

    nsInit();
    
    NS_TerminateProcessTID(INPUT_TID, 10e+9);

    u32 pid;
    NS_LaunchTitle(INPUT_TID, 0, &pid);

    nsExit();

    return 0;
}