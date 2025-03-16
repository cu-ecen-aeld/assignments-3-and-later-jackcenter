#include "socket_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "utilities.h"

int socket_client_create_connection(const int server_fd, int *client_fd_ptr,
                                    const struct timespec *timeout_ptr) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  int client_fd = -1;

  struct timespec start_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  struct timespec end_time;
  timespec_add(&start_time, timeout_ptr, &end_time);

  while (client_fd < 0) {
    if (config_is_terminated()) {
      return 1;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (timespec_is_elapsed(&end_time, &current_time)) {
      return 2;
    }

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
      struct timespec accept_delay;
      accept_delay.tv_sec = 0;
      accept_delay.tv_nsec = 10e6;
      nanosleep(&accept_delay, NULL);
      continue;
    }

    syslog(LOG_ERR, "accept: %s", strerror(errno));
    perror("accept");
    return -1;
  }

  char ip4_string[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(client_addr.sin_addr), ip4_string, INET_ADDRSTRLEN);
  syslog(LOG_INFO, "Accepted connection from %s\n", ip4_string);

  *client_fd_ptr = client_fd;
  return 0;
}

int socket_client_receive_and_write_data(char *file, const int client_fd) {
  char recv_buffer[BUFFER_SIZE];
  size_t bytes_received = 0;

  do {
    bytes_received = recv(client_fd, recv_buffer, sizeof(recv_buffer), 0);
    if (bytes_received == -1) {
      perror("recv");
      return -1;
    }

    // Add data to end of the file
    if (append_to_file(file, recv_buffer, bytes_received) == -1) {
      syslog(LOG_ERR, "append_to_file");
      return -1;
    }
  } while (bytes_received == BUFFER_SIZE);

  return 0;
}

int socket_client_send_file(char *file, const int client_fd) {
  const int fd = open(file, O_RDONLY);

  // TODO: This should be allocated to a size dynamically instead of just
  // setting the size needed to pass the test. This section needed to be
  // reworked since the driver didn't work with getline().
  size_t line_len = 32768;
  char *line = malloc(line_len);
  ssize_t bytes_read = read(fd, line, line_len);
  while (bytes_read != 0) {
    syslog(LOG_INFO, "Read bytes: %ld", bytes_read);
    if (socket_client_send_line(client_fd, line, bytes_read)) {
      syslog(LOG_ERR, "socket_client_send_line send");
      return -1;
    }
    bytes_read = read(fd, line, line_len);
  }

  free(line);
  close(fd);
  return 0;
}

int socket_client_send_line(const int client_fd, char *line, const size_t length) {
  syslog(LOG_INFO, "Sending size (%lu):  %s", length, line);
  size_t bytes_sent = send(client_fd, line, length, 0);
  if (bytes_sent == -1) {
    perror("send");
    return -1;
  }

  if (bytes_sent != length) {
    syslog(LOG_ERR, "partial send");
    return -1;
  }

  return 0;
}
