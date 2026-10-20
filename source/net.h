#pragma once
#include <stddef.h>

#define NET_MAX_CLIENTS 10

struct NetServer {
    int listen_fd;
    int num_clients;
    int clients[NET_MAX_CLIENTS];
    unsigned long long retry_at; ///< System tick before which the listener is not reopened.
};

/// Brings up the socket service. Returns false on failure.
bool netInit(NetServer* net);

/// Closes every socket and shuts the socket service down.
void netExit(NetServer* net);

/// Accepts every pending client. (Re)opens the listening socket if needed.
/// Never blocks.
void netAccept(NetServer* net);

/// Sends one message to every client. Slow clients miss the frame, dead
/// clients are dropped. Never blocks.
void netBroadcast(NetServer* net, const void* data, size_t len);
