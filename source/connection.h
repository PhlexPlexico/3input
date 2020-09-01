#ifndef CONNECTION_H
#define CONNECTION_H

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

__attribute__((format(printf,1,2)))
extern void failExit(const char *fmt, ...);


struct connection_t {
    int32_t sock;
    struct sockaddr_in socket;
};

int conn_server_ctor(struct connection_t*, int);
int conn_client_ctor(struct connection_t*, int, struct sockaddr_in*);
int conn_dtor(struct connection_t*);
ssize_t conn_send(struct connection_t*,char*, size_t);


#endif