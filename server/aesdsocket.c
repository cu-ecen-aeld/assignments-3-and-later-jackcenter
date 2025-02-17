#include <arpa/inet.h>    // inet
#include <errno.h>
#include <fcntl.h>        // creat
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>       // free
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#define PORT ("9600")
#define BACKLOG (2)
#define RESULT_FILE "/var/tmp/aesdsocketdata"

bool is_terminated = false;

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
 * @return 1 if an external termination is called
 * @return -1 otherwise
 */
static int create_client_connection(const int server_fd, int *client_fd_ptr);

/**
 * @brief Appends the `buffer` to the `file` and creates the file if it doesn't exist
 * @param file file location
 * @param buffer data to write
 * @param buffer_len size of the data in `buffer`
 * @return 0 if successful
 * @return -1 otherwise
 */
static int append_to_file(const char* file, char* buffer, const size_t buffer_len);

static int read_line_from_stream(FILE *stream, char** line, size_t *line_len);

static int send_line();

static int send_file();

void sigint_handler(int sig);

int main() {
  // Register SIGINT handler
  if (signal(SIGINT, sigint_handler) == SIG_ERR) {
    perror("signal");
    return -1;
  }
  
  // Setup the server
  int server_socket = 0;
  struct addrinfo *server_info = NULL;
  if (create_server_socket(&server_socket, &server_info) != 0) {
    syslog(LOG_ERR, "create_server_socket");
    freeaddrinfo(server_info);
    return -1;
  }

  while (!is_terminated) {
    // Wait for a connection
    int client_socket = 0;
    const int connection_result = create_client_connection(server_socket, &client_socket);
    switch (connection_result) {
      case -1:
        syslog(LOG_ERR, "create_client_connection");
        freeaddrinfo(server_info);
        return -1;
      case 1:
        continue;
      default:
    }


    // RECEIVE

    // ===== Write the Packet =====
    // Wait for packet
    char recv_buffer[1024];
    size_t bytes_received = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);
    if ( bytes_received == -1) {
      perror("recv");
      freeaddrinfo(server_info);
      return -1;
    }
    recv_buffer[bytes_received] = '\n';

    if (append_to_file(RESULT_FILE, recv_buffer, bytes_received) == -1) {
      syslog(LOG_ERR, "append_to_file");
      freeaddrinfo(server_info);
      return -1;
    }

    // ===== Send the Packet =====
    
    // // Return to the beginning of the file then send one line at a time. 
    const int fd = open(RESULT_FILE, O_RDONLY);
    FILE *stream = fdopen(fd, "r");
    if (NULL == stream) {
      syslog(LOG_ERR, "fdopen");
      freeaddrinfo(server_info);
      return -1;
    }

    // send file data
    send_file();

    // get a line
    char *line = NULL;
    size_t line_len = 0;
    int read_line_result = read_line_from_stream(stream, &line, &line_len);
    switch(read_line_result) {
      case 1: // end of file
        free(line);
        continue;
      case -1: // error
        syslog(LOG_ERR, "read_line_from_stream");
        free(line);
        freeaddrinfo(server_info);
        return -1;
      default:
    }

    printf("Line: %s\r\n", line);
 
    // send a line
    char send_buffer[1024] = "Read: ";
    strcat(send_buffer, line);
    size_t bytes_sent = send(client_socket, send_buffer, strlen(send_buffer), 0);

    if (bytes_sent == -1) {
      perror("send");
      freeaddrinfo(server_info);
      free(line);
      return -1;
    }

    if (bytes_sent != strlen(send_buffer)) {
      perror("send");
      freeaddrinfo(server_info);
      free(line);
      return -1;
    }

    // printf("Line: %s\r\n", line);
    // if (fclose(stream) != 0) {
    //   printf("Error: fclose\r\n");
    //   free(line);
    //   freeaddrinfo(server_info);
    //   return -1;
    // }

    

    // ===== Send the Packet (End) =====
    free(line);
    // TODO: close the connection
  }


  // TODO: handle this in the termination signal handler.
  // DELETE
  printf("Delete\r\n");
  freeaddrinfo(server_info);
  if (unlink(RESULT_FILE) != 0) {
    perror("unlink");
    return -1;
  } 

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
    perror("getaddrinfo");
  }

  return status;
}

int create_server_socket(int *socket_fd_ptr, struct addrinfo **address_info) {
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

  if (create_server_address_info(address_info) != 0) {
    syslog(LOG_ERR, "create_server_address_info");
    return -1;
  }

  if (bind(socket_fd, (*address_info)->ai_addr, (*address_info)->ai_addrlen) == -1) {
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

int create_client_connection(const int server_fd, int *client_fd_ptr) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  int client_fd = -1;
  while (client_fd < 0) {
    if (is_terminated) {
      return 1;
    }

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
    {
      // TODO: add delay
      continue;
    }
  
    printf("Error: accept\r\n");
    return -1;
  }
   
  char ip4_string[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(client_addr.sin_addr), ip4_string, INET_ADDRSTRLEN);
  syslog(LOG_INFO, "Accepted connection from %s\n", ip4_string);

  *client_fd_ptr = client_fd;
  return 0;
}

int append_to_file(const char* file, char* buffer, const size_t buffer_len) {
  const int fd = open(file, O_WRONLY | O_APPEND | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
  const ssize_t result = write(fd, buffer, buffer_len);
  if (result == -1) {
    perror("write");
    return -1;
  } else if (result != buffer_len) {
    syslog(LOG_ERR, "partial write");
    return -1;
  }

  if (close(fd) == -1) {
    perror("close");
    return -1;
  }

  return 0;
}

int read_line_from_stream(FILE *stream, char** line, size_t *line_len) {
  if (getline(line, line_len, stream) == -1) {
    if (feof(stream)) {
      return 1;
    }

    syslog(LOG_ERR, "getline");
    return -1;
  }

  return 0;
}

int send_line() {
  return 0;
}

int send_file() {
  return send_line();
}

void sigint_handler(int sig) {
  printf("Ctrl+C!\r\n");
  is_terminated = true; 
}