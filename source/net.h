#ifndef NET_H
#define NET_H
#include <3ds.h>

#include "connection.h"

#define MAX_CONNECTIONS 10

/// Structure for networks. 
typedef struct {
    struct connection_t server; ///< Connection structure which contains the server.

    LightLock connection_lock; ///< Connection lock.

    int32_t num_clients; ///< Current number of clients.
    struct connection_t clients[MAX_CONNECTIONS]; ///< Array of clients connected to server.
} net_t;

/**
 * @brief Constructor for the network structure. Inits the LightLock in the struct.
 * @param net pointer to a struct of net_t.
 * @return 0 on sucess of net constructing. -1 on failure.
 */
int net_ctor(net_t *);

/**
 * @brief Destructor for the network structure. Sends message to clients and removes connections.
 * @param net pointer to a struct of net_t.
 */
void net_dtor(net_t *);

/**
 * @brief Adds a client to the clients array. If a display is configured, 
 * it outputs to the top screen that a client was added.
 * @param net pointer to a struct of net_t.
 * @return 0 on successful add.
 */
int net_accept(net_t *);

/**
 * @brief Sends a message to all connected clients.
 * @param net pointer to a struct of net_t.
 * @param data message to send to the clients.
 * @param size the size of the message.
 */
void net_send(net_t *, char*, size_t);

/**
 * @brief Calls a lock on the connection_lock.
 * @param net pointer to a struct of net_t.
 */
void net_lock(net_t *);

/**
 * @brief Calls an unlock on the connection_lock.
 * @param net pointer to a struct of net_t.
 */
void net_unlock(net_t *);

/**
 * Exits and re-inits a socket.
*/
void net_resoc();
#endif