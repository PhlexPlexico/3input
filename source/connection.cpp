#include "connection.h"

#include <errno.h>
#include <string.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>

#include "ui.h"


int conn_server_ctor(struct connection_t* conn, int max) {
    int ret;
    
    conn->sock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (conn->sock < 0) {
        return -1;
	}

    conn->socket.sin_family = AF_INET;
	conn->socket.sin_port = htons (65534);
	conn->socket.sin_addr.s_addr = gethostid();

    if ( (ret = bind (conn->sock, (struct sockaddr *) &(conn->socket), sizeof (struct sockaddr_in))) ) {
		close(conn->sock);
        return -1;
	}

    // Set socket non blocking so we can still read input to exit
	fcntl(conn->sock, F_SETFL, fcntl(conn->sock, F_GETFL, 0) | O_NONBLOCK);


    if ( (ret = listen(conn->sock, max)) ) {
        return -1;
	}

    return 0;

}

int conn_client_ctor(struct connection_t* conn, int sock, struct sockaddr_in* socket) {
    conn->sock = sock;
    memcpy(&(conn->socket), socket, sizeof(struct sockaddr_in));

    return 0;
}

int conn_dtor(struct connection_t* conn) {
    close(conn->sock);
    memset(conn, 0, sizeof(struct connection_t));

    return 0;
}

ssize_t conn_send(struct connection_t* conn, char* data, size_t len) {
    return send(conn->sock, data, len, 0);
}