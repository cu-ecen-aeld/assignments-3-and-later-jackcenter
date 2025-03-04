#ifndef SOCKET_CLIENT
#define SOCKET_CLIENT

#include <time.h>

/**
 * @brief Creates the client connection
 * @param server_fd server socket file descriptor
 * @param client_fd_ptr pointer to the location to store the client file
 * descriptor.
 * @return 0 if successful
 * @return 1 if an external termination is called
 * @return -1 otherwise
 */
int socket_client_create_connection(const int server_fd, int *client_fd_ptr,
                                    const struct timespec *timeout);

/**
 * @brief Receives data from client and writes to file
 * @param file file to send
 * @param client_fd client socket
 * @return 0 if successful
 * @return -1 otherwise
 */
int socket_client_receive_and_write_data(char *file, const int client_fd);

/**
 * @brief Sends the contents of `file` to the `client_fd` one line at a time
 * @param file file to send
 * @param client_fd client socket
 * @return 0 if successful
 * @return -1 otherwise
 */
int socket_client_send_file(char *file, const int client_fd);

/**
 * @brief Sends the `line` to the `client_fd`
 * @param client_fd client socket
 * @param line string to send to the client
 * @return 0 if successful
 * @return -1 otherwise
 */
int socket_client_send_line(const int client_fd, char *line);

#endif // SOCKET_CLIENT
