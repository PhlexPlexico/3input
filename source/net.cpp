#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <3ds.h>

#include "protocol.h"

namespace {

constexpr size_t SOC_ALIGN       = 0x1000;
constexpr size_t SOC_BUFFER_SIZE = 0x50000;

/// Wait this long before retrying a failed listener (wifi down, port busy...).
constexpr u64 LISTEN_RETRY_TICKS = SYSCLOCK_ARM11 * 2;

u32* socBuffer = nullptr;

bool wouldBlock(int err) {
    return err == EAGAIN || err == EWOULDBLOCK;
}

bool setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

void closeListener(NetServer* net) {
    if (net->listen_fd >= 0) {
        close(net->listen_fd);
        net->listen_fd = -1;
    }
}

void removeClient(NetServer* net, int i) {
    close(net->clients[i]);
    net->clients[i] = net->clients[--net->num_clients];
}

bool openListener(NetServer* net) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        return false;

    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(INPUT_PORT);
    addr.sin_addr.s_addr = gethostid();

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        listen(fd, NET_MAX_CLIENTS) < 0 ||
        !setNonBlocking(fd)) {
        close(fd);
        return false;
    }

    net->listen_fd = fd;
    return true;
}

} // namespace

bool netInit(NetServer* net) {
    memset(net, 0, sizeof(*net));
    net->listen_fd = -1;

    socBuffer = static_cast<u32*>(memalign(SOC_ALIGN, SOC_BUFFER_SIZE));
    if (!socBuffer)
        return false;
    if (R_FAILED(socInit(socBuffer, SOC_BUFFER_SIZE))) {
        free(socBuffer);
        socBuffer = nullptr;
        return false;
    }
    return true;
}

void netExit(NetServer* net) {
    while (net->num_clients)
        removeClient(net, net->num_clients - 1);
    closeListener(net);
    socExit();
    free(socBuffer);
    socBuffer = nullptr;
}

void netReset(NetServer* net) {
    while (net->num_clients)
        removeClient(net, net->num_clients - 1);
    closeListener(net);
    net->retry_at = 0;
}

void netAccept(NetServer* net) {
    if (net->listen_fd < 0) {
        u64 now = svcGetSystemTick();
        if (now < net->retry_at)
            return;
        if (!openListener(net)) {
            net->retry_at = now + LISTEN_RETRY_TICKS;
            return;
        }
    }

    while (net->num_clients < NET_MAX_CLIENTS) {
        sockaddr_in peer;
        socklen_t   peer_len = sizeof(peer);
        int fd = accept(net->listen_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
        if (fd < 0) {
            // Nothing pending is the normal case; anything else means the
            // listener is dead (e.g. wifi dropped), so rebuild it later.
            if (!wouldBlock(errno)) {
                closeListener(net);
                net->retry_at = svcGetSystemTick() + LISTEN_RETRY_TICKS;
            }
            return;
        }
        // A client that stops reading must never stall the input loop.
        if (!setNonBlocking(fd)) {
            close(fd);
            continue;
        }
        net->clients[net->num_clients++] = fd;
    }
}

void netBroadcast(NetServer* net, const void* data, size_t len) {
    for (int i = 0; i < net->num_clients;) {
        ssize_t sent = send(net->clients[i], data, len, 0);
        // Sent in full, or the client's buffer is full and it just misses
        // this frame. A short write would desync the stream, so it counts as
        // a dead client like any other error.
        if (sent == static_cast<ssize_t>(len) || (sent < 0 && wouldBlock(errno)))
            i++;
        else
            removeClient(net, i);
    }
}
