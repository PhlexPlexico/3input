#include "net.h"

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32 *SOC_buffer = NULL;

void socShutdown() {
	printf("waiting for socExit...\n");
	socExit();
}

void init_soc_stuff(void) {
    int ret;

	//while(1){svcSleepThread(0);hidScanInput();if(hidKeysHeld() & KEY_START) break;}


    SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

    if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

    if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}

    atexit(socShutdown);
}

void net_resoc() {
    socExit();
    free(SOC_buffer);
    init_soc_stuff();
}


int net_ctor(net_t * net) {
    int ret;
    
    if(!SOC_buffer) {
        init_soc_stuff();
    }

    memset(net, 0, sizeof(net_t));

    ret = conn_server_ctor(&(net->server), MAX_CONNECTIONS);

    if(ret){
        return ret;
    }

    LightLock_Init(&(net->connection_lock));

    return 0;
}

void net_dtor(net_t * net) {
    static char disconnecting[] = "\n3ds disconnecting\n";
    net_send(net, disconnecting, strlen(disconnecting));
    while(net->num_clients) {
        conn_dtor(&(net->clients[net->num_clients--]));
    }
    conn_dtor(&(net->server));
}

int net_accept(net_t* net) {
    static uint32_t soclen = sizeof(struct sockaddr_in);

    struct connection_t client;

    if(net->num_clients >= MAX_CONNECTIONS){
        return 0;
    }

    memset(&client, 0, sizeof(struct connection_t));

    client.sock = accept(net->server.sock, (struct sockaddr *) &(client.socket), &soclen);

    if(client.sock < 0){
        return client.sock;
    }

    net_lock(net);
    conn_client_ctor(&(net->clients[net->num_clients++]), client.sock, &(client.socket));
    net_unlock(net);

    printf_top("[+]Added client %ld\n", net->num_clients);

    return 0;
}

void net_remove(net_t *net, int i) {
    int num_to_move;
    struct connection_t* client,* next_client;


    client = &(net->clients[i]);
    next_client = &(net->clients[i+1]);
                
    conn_dtor(client);

    num_to_move = net->num_clients - (i + 1);

    memmove(client, next_client, num_to_move * sizeof(struct connection_t));

    net->num_clients--;
    printf_top("[-]Removed client %d, now down to %ld\n", i, net->num_clients);
}

void net_send(net_t *net, char* data, size_t len) {
    if(net->num_clients == 0) {
        return;
    }
    
    net_lock(net);

    for(int i = 0; i < net->num_clients; i++) {
        int ret;
        struct connection_t* client;

        client = &(net->clients[i]);
        ret = conn_send(client, data, len);

        if(ret < 0) {
            //printf_top("conn_send: %d", errno);
            if(errno == ECONNRESET || errno == EINPROGRESS) {
                net_remove(net, i);
                i--;
            }
            else {
                //TODO: unknown error
            }
        }
    }

    net_unlock(net);
}

void net_lock(net_t *net) {
    //LightLock_Lock(&(net->connection_lock));
}

void net_unlock(net_t *net) {
    //LightLock_Unlock(&(net->connection_lock));
}
