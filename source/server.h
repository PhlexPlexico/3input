#ifndef SERVER_H

#include <3ds.h>

#include "net.h"

typedef void (*sender_func_t)(net_t*, void*);

typedef struct {
    net_t net;


    sender_func_t sender_func;
    void* sender_info;
    size_t server_info_size;

    int count;
    int accept_skip;

    Handle server_timer;
    Thread server_thread;
    LightEvent  exit_thread;
    aptHookCookie cookie;
} server_t;

/**
 * @brief Constructor for a new server. Creates a newm non-detached thread.
 * @param server pointer to a struct of server_t.
 * @param func server functionality of type sender_func_t. e.g. found in servertypes, an input redirect.
 * @param size size of function sender_func_t.
 * @return 0 on sucess of thread creation.
 */
int server_ctor(server_t*, sender_func_t, size_t);

/**
 * @brief Destructor for a server.
 * @param server pointer to a struct of server_t.
 * @return 0 on sucess of thread creation.
 */
int server_dtor(server_t*);

/**
 * @brief Changes server send frequency.
 * @param server pointer to a struct of server_t.
 * @param num int to set frequency to per num seconds.
 * @param dev int to set frequency of times per num.
 * @param scr Console to print to.
 * @return 0 on sucess of thread creation.
 */
void server_change_timer_freq(server_t*, int, int, PrintConsole*);


#define SERVER_H
#endif