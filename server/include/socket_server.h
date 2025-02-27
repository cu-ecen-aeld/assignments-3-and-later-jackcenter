#ifndef SOCKET_SERVER_H
#define SOCKET_SERVER_H

#include <netdb.h>

/**
 * @brief Creates the server socket.
 * @param socket_fd_ptr pointer to the location to store the file descriptor
 * @param address_info pointer to the pointer to the stuct to hold the info
 * @return 0 if successful
 * @return -1 otherwise
 */
int socket_server_create(int *socket_fd_ptr, struct addrinfo **address_info);

/**
 * @brief Creates the `addrinfo` for the server.
 * @param address_info pointer to the pointer to the stuct to hold the info
 * @return 0 if successful
 * @return getaddrinfo error number otherwise
 */
int socket_server_create_address_info(struct addrinfo **address_info);

#endif  // SOCKET_SERVER_H
