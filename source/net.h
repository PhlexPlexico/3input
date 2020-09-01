#ifndef NET_H
#define NET_H
#include <3ds.h>

#include "connection.h"

#define MAX_CONNECTIONS 10

typedef struct {
    struct connection_t server;

    LightLock connection_lock;

    int32_t num_clients;
    struct connection_t clients[MAX_CONNECTIONS];
} net_t;

int net_ctor(net_t *);
void net_dtor(net_t *);

int net_accept(net_t *);
void net_send(net_t *, char*, size_t);

void net_lock(net_t *);
void net_unlock(net_t *);

void net_resoc();
#endif