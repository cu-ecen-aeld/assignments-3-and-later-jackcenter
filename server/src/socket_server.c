#include "socket_server.h"

#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>

#include "config.h"

int socket_server_create(int *socket_fd_ptr, struct addrinfo **address_info) {
  const int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (socket_fd == -1) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt");
    return -1;
  }

  if (socket_server_create_address_info(address_info) != 0) {
    syslog(LOG_ERR, "create_server_address_info");
    return -1;
  }

  if (bind(socket_fd, (*address_info)->ai_addr, (*address_info)->ai_addrlen) ==
      -1) {
    perror("bind");
    return -1;
  }

  if (listen(socket_fd, BACKLOG) == -1) {
    perror("listen");
    return -1;
  }

  *socket_fd_ptr = socket_fd;
  return 0;
}

int socket_server_create_address_info(struct addrinfo **address_info) {
  struct addrinfo address_hints;
  memset(&address_hints, 0, sizeof(address_hints));
  address_hints.ai_family = AF_UNSPEC;
  address_hints.ai_socktype = SOCK_STREAM;
  address_hints.ai_flags = AI_PASSIVE;

  int status = 0;
  if ((status = getaddrinfo(NULL, PORT, &address_hints, address_info)) != 0) {
    perror("getaddrinfo");
  }

  return status;
}
