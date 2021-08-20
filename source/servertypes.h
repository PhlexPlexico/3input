#ifndef SERVERTYPES_H
#define SERVERTYPES_H

#include "server.h"

/**
 * @brief Creates the input viewer server. 
 * If HID is not initialized, borrow from sharedMemory.
 * @param server pointer to a struct of server_t.
 * @param frequency The initial starting frequency.
 * @return 0 on sucess of thread creation.
 */
int make_input_server(server_t*, int);

#endif