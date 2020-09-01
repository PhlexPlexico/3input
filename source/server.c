#include "server.h"

#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "ui.h"


#define PAGE_SIZE 0x1000
#define STACK_SIZE (1 * PAGE_SIZE)

#define SERVER_PRIO   0x3f

#define  S(c)           (((uint64_t)c * (uint64_t)1000000000))
#define PER_S(c)        ( S(1) / c)

#define log(x)          ret = x;\
                        printf("%d = " #x "\n",ret);

void server_main(void*);
void server_exiting_thuc(net_t*, void*);

void aptServerHook(APT_HookType, void*);


int server_ctor(server_t* server, sender_func_t func, size_t size){
    //aptHook(&(server->cookie), aptServerHook, server);
    //s32 prio = 0;
	//svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    memset(server, 0, sizeof(server_t));
    net_ctor(&(server->net));
    
    server->sender_func = func;
    server->sender_info = malloc(size);
    server->server_info_size = size;

    server->count = 0;
    server->accept_skip = 2;

    svcCreateTimer(&(server->server_timer), RESET_PULSE);
    svcSetTimer(server->server_timer, 0, PER_S(10));
    LightEvent_Init(&(server->exit_thread), RESET_STICKY);
    LightEvent_Clear(&(server->exit_thread));

    server->server_thread   = threadCreate(  server_main, server, STACK_SIZE, SERVER_PRIO, -2, false);
    
    return 0;
}

int server_kill(server_t* server){
    
	LightEvent_Signal(&(server->exit_thread));
    
    threadJoin(server->server_thread, -1);

    
    net_dtor(&(server->net));
    svcCancelTimer(server->server_timer);
    svcClearTimer(server->server_timer);
    svcCloseHandle(server->server_timer);

    free(server->sender_info);

    return 0;
}

int server_dtor(server_t* server){
    
    server_kill(server);
	aptUnhook(&(server->cookie));
    return 0;
}

#define likely(x)      __builtin_expect(!!(x), 1)

void server_main(void* server_ptr) {
    bool keep_running;
    server_t * server = (server_t*) server_ptr;
    keep_running = true;
    while(likely(keep_running)) {
        svcWaitSynchronization(server->server_timer, S(3));
        
        if((server->count)++ == server->accept_skip) {
            server->count = 0;
            net_accept(&(server->net));
            //DEBUG: Print the tick the server is sending on its child thread.
            //printf("tick %d\t%d\n",server->count, keep_running);
        }
        
        server->sender_func(&(server->net), server->sender_info);
        keep_running = !LightEvent_TryWait(&(server->exit_thread));
    }
    threadExit(0);
}

void server_exiting_thuc(net_t* net, void* v) {
    threadExit(0);
}

static int reopen_soc = 0;
void onSleep(server_t* server) {
    server_kill(server);
    //printf("server killed\n");
    AtomicSwap(&reopen_soc, 1);
}

void onWake(server_t* server) {
    //printf("On Wake\n");
    if(reopen_soc) {
        net_resoc();
        AtomicSwap(&reopen_soc, 0);
        //("resoc\n");
    }
    server_ctor(server, server->sender_func, server->server_info_size);    
}

void aptServerHook(APT_HookType hook, void* server_ptr) {
    server_t* server = (server_t*) server_ptr;
    switch(hook) {
        case  APTHOOK_ONSLEEP:
            onSleep(server);
            return;
        case  APTHOOK_ONWAKEUP:
            onWake(server);
            return;
        default:
            return;
    }
}


void server_change_timer_freq(server_t* server, int num, int dev, PrintConsole* scr) {
    svcCancelTimer(server->server_timer);
    svcClearTimer(server->server_timer);
    if(scr != NULL) 
    {
        consoleSelect(scr);
        printf("[*] Changing timer to %d / %d times per second\n", dev, num);
    }
    svcSetTimer(server->server_timer, 0, (int64_t)num * PER_S(1) / (int64_t)dev);

}