#ifndef CONNECTION_H
#define CONNECTION_H

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

__attribute__((format(printf,1,2)))
extern void failExit(const char *fmt, ...);


/// Structure for the network connection. 
struct connection_t {
    int32_t sock;
    struct sockaddr_in socket;
};

/**
 * @brief Constructor for a new server connection.
 * @param conn pointer to a struct of connection_t.
 * @param max int value representing the max connections.
 * @return 0 on sucess of server connection.
 */
int conn_server_ctor(struct connection_t*, int);

/**
 * @brief Constructor for a new client connection.
 * @param sock Integer for socket file descriptor.
 * @param socket Internet address (sockaddr_in).
 * @return 0 on sucess of client connection.
 */
int conn_client_ctor(struct connection_t*, int, struct sockaddr_in*);

/**
 * @brief Destructor for client connection.
 * @param conn pointer to a struct of connection_t.
 * @return 0 on sucess of client closing.
 */
int conn_dtor(struct connection_t*);

/**
 * @brief Sends data to a client.
 * @param conn pointer to a struct of connection_t.
 * @param data data to send to client.
 * @param len size_t of the data.
 * @return success of send.
 */
ssize_t conn_send(struct connection_t*,char*, size_t);


#endif