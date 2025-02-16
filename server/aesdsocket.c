#include <arpa/inet.h>    // inet
#include <errno.h>
#include <fcntl.h>        // creat
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>       // free
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#define PORT ("9600")
#define BACKLOG (2)
#define RESULT_FILE ("/var/tmp/aesdsocketdata")

/**
 * @brief Creates the `addrinfo` for the server.
 * @param address_info pointer to the pointer to the stuct to hold the info
 * @return 0 if successful
 * @return getaddrinfo error number otherwise
 */
static int create_server_address_info(struct addrinfo **address_info);

/**
 * @brief Creates the server socket.
 * @param socket_fd_ptr pointer to the location to store the file descriptor
 * @param address_info pointer to the pointer to the stuct to hold the info
 * @return 0 if successful
 * @return -1 otherwise
 */
static int create_server_socket(int *socket_fd_ptr, struct addrinfo **address_info);

/**
 * @brief Creates the client connection
 * @param server_fd server socket file descriptor
 * @param client_fd_ptr pointer to the location to store the client file descriptor.
 * @return 0 if successful
 * @return -1 otherwise
 */
static int create_client_connection(const int server_fd, int *client_fd_ptr);

int main() {
  
  // Setup the server
  int server_socket = 0;
  struct addrinfo *server_info = NULL;
  if (create_server_socket(&server_socket, &server_info) != 0) {
    syslog(LOG_ERR, "create_server_socket");
    freeaddrinfo(server_info);
    return -1;
  }

  // Wait for a connection
  if (listen(server_socket, BACKLOG) == -1) {
    perror("bind");
    freeaddrinfo(server_info);
    return -1;
  }

  // Accept a connection
  int client_socket = 0;
  if (create_client_connection(server_socket, &client_socket) != 0) {
    syslog(LOG_ERR, "create_client_connection");
    freeaddrinfo(server_info);
    return -1;
  }

  const int result_fd = open(RESULT_FILE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR |S_IRGRP | S_IROTH);
  
  // WRITE
  // TODO: replace with data recieved from connections. Buffer data until an '/n' is received.
  printf("Write\r\n");
  char data[] = "Hello File\n";
  const size_t data_len = strlen(data);

  const ssize_t nr = write(result_fd, data, data_len);
  if (nr == -1) {
    const int err = errno;
    printf("Error: write - %d: %s\r\n", err, strerror(err));
    freeaddrinfo(server_info);
    return -1;
  } else if (nr != data_len) {
    printf("Error: partial write\r\n");
    freeaddrinfo(server_info);
    return -1;
  }

  // READ
  printf("Read\r\n");

  // Return to the beginning of the file then send one line at a time. 
  // TODO: send each line to the client.
  lseek(result_fd, 0, SEEK_SET);

  FILE *stream = fdopen(result_fd, "r");
  if (NULL == stream) {
    printf("Error: fdopen\r\n");
    freeaddrinfo(server_info);
    return -1;
  }

  printf("1\r\n");
  char *line = NULL;
  size_t line_len = 0;
  if (getline(&line, &line_len, stream) == -1) {
    printf("Error: getline\r\n");
    free(line);
    freeaddrinfo(server_info);
    return -1;
  }

  printf("Line: %s\r\n", line);

  printf("2\r\n");
  if (fclose(stream) != 0) {
    printf("Error: fclose\r\n");
    free(line);
    freeaddrinfo(server_info);
    return -1;
  }

  // TODO: close the connection
  // TODO: loop to wait for another connection

  // TODO: handle this in the termination signal handler.
  // DELETE
  printf("Delete\r\n");
  if (unlink(RESULT_FILE) != 0) {
    printf("Error: unlink\r\n");
    free(line);
    freeaddrinfo(server_info);
    return -1;
  } 

  free(line);
  freeaddrinfo(server_info);
  return 0;
}

int create_server_address_info(struct addrinfo **address_info) {
  struct addrinfo address_hints;
  memset(&address_hints, 0, sizeof(address_hints));
  address_hints.ai_family = AF_UNSPEC;
  address_hints.ai_socktype = SOCK_STREAM;
  address_hints.ai_flags = AI_PASSIVE;

  int status = 0;
  if ((status = getaddrinfo(NULL, PORT, &address_hints, address_info)) != 0) {
    printf("Did this work?\r\n");
    perror("getaddrinfo");
  }

  return status;
}

int create_server_socket(int *socket_fd_ptr, struct addrinfo **address_info) {
  const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    perror("socket");
    return -1; 
  }

  if (create_server_address_info(address_info) != 0) {
    syslog(LOG_ERR, "create_server_address_info");
    return -1;
  }

  if (bind(socket_fd, (*address_info)->ai_addr, (*address_info)->ai_addrlen) == -1) {
    perror("bind");
    return -1;
  }

  *socket_fd_ptr = socket_fd; 
  return 0;
}

int create_client_connection(const int server_fd, int *client_fd_ptr) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);
  const int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
  if (client_fd == -1) {
    printf("Error: accept\r\n");
    return -1;
  }

  char ip4_string[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(client_addr.sin_addr), ip4_string, INET_ADDRSTRLEN);
  syslog(LOG_INFO, "Accepted connection from %s\n", ip4_string);

  *client_fd_ptr = client_fd;
  return 0;
}