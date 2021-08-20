#ifndef SERVER_H

#include <3ds.h>

#include "net.h"

typedef void (*sender_func_t)(net_t*, void*);

typedef struct {
    net_t net; ///< Network structure.


    sender_func_t sender_func; ///< Stored function that the server will run.
    void* sender_info; ///< Information of the sender.
    size_t server_info_size; ///< Size of sender info.

    int count; ///< ???
    int accept_skip; ///< Acceptable skip for each tick.

    Handle server_timer; ///< svcHandle for a timer. Used for server sending items/second.
    Thread server_thread; ///< Server thread to process the sender function.
    LightEvent  exit_thread; ///< Event to keep the server running. 
    aptHookCookie cookie; ///< Unused - Applet Cookie Hook.
} server_t;

/**
 * @brief Constructor for a new server. Creates a newm non-detached thread.
 * @param server pointer to a struct of server_t.
 * @param func server functionality of type sender_func_t. e.g. found in servertypes, an input redirect.
 * @param size size of function sender_func_t.
 * @param frequency the initial polling per second frequency.
 * @return 0 on sucess of thread creation.
 */
int server_ctor(server_t*, sender_func_t, size_t, int);

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