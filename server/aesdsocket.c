#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "queue.h"

#define PORT "9000"
#define BACKLOG (2)
#define RESULT_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE (1024)

typedef enum { UNITITIALIZED, RUNNING, SUCCEEDED, FAILED } ThreadStatus;

typedef struct {
  int client_fd;
  ThreadStatus thread_status;
} ThreadData;

struct node {
  ThreadData thread_data;
  pthread_t thread;
  SLIST_ENTRY(node) nodes;
};

struct head_s {
  struct node *slh_first; /* first element */
};

bool is_terminated = false;

/**
 * @brief Runs the socket application. This application sets up a server that
 * waits for client connections, writes data from the client to file, then sends
 * the contents of the file back to the client. It uses a threaded approach to
 * allow multiple clients connect simultaneously.
 * @param server_fd filed descriptor for the server
 * @return 0 if successful
 * @return -1 otherwise
 */
static int application(const int server_fd);

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
static int create_server_socket(int *socket_fd_ptr,
                                struct addrinfo **address_info);

/**
 * @brief Sets up the daemon to run the program
 * @return 0 if successful (in daemon process)
 * @return 1 if in parent process
 * @return -1 otherwise
 */
static int daemonize(void);

/**
 * @brief Creates the client connection
 * @param server_fd server socket file descriptor
 * @param client_fd_ptr pointer to the location to store the client file
 * descriptor.
 * @return 0 if successful
 * @return 1 if an external termination is called
 * @return -1 otherwise
 */
static int create_client_connection(const int server_fd, int *client_fd_ptr,
                                    const struct timespec *timeout);

/**
 * @brief Appends the `buffer` to the `file` and creates the file if it doesn't
 * exist
 * @param file file location
 * @param buffer data to write
 * @param buffer_len size of the data in `buffer`
 * @return 0 if successful
 * @return -1 otherwise
 */
static int append_to_file(const char *file, char *buffer,
                          const size_t buffer_len);

/**
 * @brief Receives data from client and writes to file
 * @param file file to send
 * @param client_fd client socket
 * @return 0 if successful
 * @return -1 otherwise
 */
static int receive_and_write_data(char *file, const int client_fd);

/**
 * @brief Gets a line from the `stream`
 * @param stream file stream
 * @param line pointer to the location of the line string
 * @param line_len pointer to location to stor the line length
 * @return 0 if successful
 * @return 1 if at the end of the file
 * @return -1 otherwise
 */
static int read_line_from_stream(FILE *stream, char **line, size_t *line_len);

/**
 * @brief Sends the `line` to the `client_fd`
 * @param client_fd client socket
 * @param line string to send to the client
 * @return 0 if successful
 * @return -1 otherwise
 */
static int send_line(const int client_fd, char *line);

/**
 * @brief Sends the contents of `file` to the `client_fd` one line at a time
 * @param file file to send
 * @param client_fd client socket
 * @return 0 if successful
 * @return -1 otherwise
 */
static int send_file(char *file, const int client_fd);

/**
 * @brief SIGINT signal handler used to gracefully shutdown the program
 * @param sig signal number
 */
static void sigint_handler(int sig);

/**
 * @brief Intended to be run in a thread. Completes the data transfer from the
 * client passed in `arg`. Data from the client is recieved and written to file,
 * then the entire contents of the file are sent back to the client.
 * @param arg client file descriptor as an int*
 */
static void *data_transfer(void *arg);

/**
 * @brief Adds the `lhs` and `rhs` timespecs together and puts them in `result`.
 */
void add_timespec(const struct timespec *lhs, const struct timespec *rhs,
                  struct timespec *result);

/**
 * @brief returns true if the `current_time` is greater than the `end_time`.
 */
bool is_timespec_elapsed(const struct timespec *end_time,
                         const struct timespec *current_time);

/**
 * @brief Joins any thread in the linked list `head` that is complete.
 * @return true if successful
 * @return false otherwise
 */
bool join_completed_threads(struct head_s *head);

/**
 * @brief Waits for all threads in the linked list `head` to join.
 * @return true if successful
 * @return false otherwise
 */
bool join_threads(struct head_s *head);

int main(int argc, char *argv[]) {
  openlog("aesdsocket", LOG_PID, LOG_USER);

  // Parse Arguments
  bool execute_as_daemon = false;
  if ((argc == 2) && (strcmp(argv[1], "-d") == 0)) {
    execute_as_daemon = true;
  } else if (argc != 1) {
    printf("Usage: %s [-d]\r\n", argv[0]);
    printf("Options:\r\n");
    printf("  -d    execute as deamon\r\n");
    closelog();
    return 1;
  }

  // Register SIGINT handler
  if (signal(SIGINT, sigint_handler) == SIG_ERR) {
    perror("signal");
    closelog();
    return -1;
  }

  // Register SIGTERM handler
  if (signal(SIGTERM, sigint_handler) == SIG_ERR) {
    perror("signal");
    closelog();
    return -1;
  }

  // Setup the socket server
  int server_socket = 0;
  struct addrinfo *server_addrinfo = NULL;
  if (create_server_socket(&server_socket, &server_addrinfo) != 0) {
    syslog(LOG_ERR, "create_server_socket");
    close(server_socket);
    return -1;
  }

  // Handle daemon execution if selected
  if (execute_as_daemon) {
    const int daemon_resut = daemonize();
    switch (daemon_resut) {
    case -1:
      syslog(LOG_ERR, "daemonize");
      close(server_socket);
      return -1;
    case 1:
      exit(EXIT_SUCCESS);
    default:
    }
  }

  // Run the application
  int result = application(server_socket);

  // Clean up
  close(server_socket);
  freeaddrinfo(server_addrinfo);
  closelog();
  return result;
}

int application(const int server_fd) {
  syslog(LOG_DEBUG, "Starting `aesdsocket`.");
  // === (AS6-3) Initialize a clock with a 10 s loop
  // Write callback function to append timestamp

  // Initialize a queue
  struct head_s head;
  SLIST_INIT(&head);

  // Loop until termination signal is received
  while (!is_terminated) {
    // Wait for a connection
    int client_socket = 0;
    struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
    const int connection_result =
        create_client_connection(server_fd, &client_socket, &timeout);
    switch (connection_result) {
    case -1: // error
      syslog(LOG_ERR, "create_client_connection");
      close(client_socket);
      return -1;
    case 0: // connection created
      break;
    case 1: // external termination
      continue;
    case 2: { // connection timeout
      if(!join_completed_threads(&head)) {
        syslog(LOG_ERR, "close_completed_threads");
      }
      continue;
    }
    default:
      syslog(LOG_ERR,
             "Unknown return code (%d) from `create_client_connection`",
             connection_result);
      continue;
    }

    // Setup thread data
    struct node *slist_data = malloc(sizeof(struct node));
    slist_data->thread_data.client_fd = client_socket;
    slist_data->thread_data.thread_status = UNITITIALIZED;

    // Complete socket data transfer in thread
    pthread_t thread_id;
    const int pthread_create_result = pthread_create(
        &thread_id, NULL, data_transfer, (void *)&(slist_data->thread_data));
    if (pthread_create_result != 0) {
      syslog(LOG_ERR, "pthread_create returned error: %d",
             pthread_create_result);
      free(slist_data);
      close(client_socket);
      return -1;
    }

    // Add thread to linked list
    slist_data->thread = thread_id;
    SLIST_INSERT_HEAD(&head, slist_data, nodes);
  }

  // Wait for all threads to join
  join_threads(&head);

  // Delete the file
  if (remove(RESULT_FILE) != 0) {
    perror("remove");
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

int daemonize(void) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  } else if (pid != 0) {
    // This process is the parent
    return 1;
  }

  // Create a new session and process group
  if (setsid() == -1) {
    syslog(LOG_ERR, "setsid");
    return -1;
  }

  // Set working directory to root
  if (chdir("/") == -1) {
    syslog(LOG_ERR, "chdir");
    return -1;
  }

  // Note: not closing files to keep server socket open.

  open("/dev/null", O_RDWR); // stdin
  dup(0);                    // stdout
  dup(0);                    // stderror

  return 0;
}

int create_client_connection(const int server_fd, int *client_fd_ptr,
                             const struct timespec *timeout_ptr) {
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  int client_fd = -1;

  struct timespec start_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  struct timespec end_time;
  add_timespec(&start_time, timeout_ptr, &end_time);

  while (client_fd < 0) {
    if (is_terminated) {
      return 1;
    }

    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (is_timespec_elapsed(&end_time, &current_time)) {
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

int append_to_file(const char *file, char *buffer, const size_t buffer_len) {
  const int fd = open(file, O_WRONLY | O_APPEND | O_CREAT,
                      S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
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

static int receive_and_write_data(char *file, const int client_fd) {
  char recv_buffer[BUFFER_SIZE];
  size_t bytes_received = 0;

  // TODO: Could add a timeout here as well
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

int read_line_from_stream(FILE *stream, char **line, size_t *line_len) {
  if (getline(line, line_len, stream) == -1) {
    if (feof(stream)) {
      return 1;
    }

    syslog(LOG_ERR, "getline");
    return -1;
  }

  return 0;
}

int send_line(const int client_fd, char *line) {
  size_t bytes_sent = send(client_fd, line, strlen(line), 0);
  if (bytes_sent == -1) {
    perror("send");
    return -1;
  }

  if (bytes_sent != strlen(line)) {
    syslog(LOG_ERR, "partial send");
    return -1;
  }

  return 0;
}

int send_file(char *file, const int client_fd) {
  const int fd = open(file, O_RDONLY);
  FILE *stream = fdopen(fd, "r");
  if (NULL == stream) {
    syslog(LOG_ERR, "fdopen");
    return -1;
  }

  char *line = NULL;
  size_t line_len = 0;
  int read_line_result = 0;
  do {
    read_line_result = read_line_from_stream(stream, &line, &line_len);
    switch (read_line_result) {
    case 1: // end of file
      continue;
    case -1: // error
      syslog(LOG_ERR, "read_line_from_stream");
      free(line);
      fclose(stream);
      return -1;
    default:
    }

    if (send_line(client_fd, line) == -1) {
      syslog(LOG_ERR, "send_line");
      free(line);
      fclose(stream);
      return -1;
    }
  } while (read_line_result != 1);

  free(line);
  fclose(stream);
  return 0;
}

void sigint_handler(int sig) {
  syslog(LOG_DEBUG, "termination signal received");
  is_terminated = true;
}

void *data_transfer(void *arg) {
  ThreadData *thread_data = (ThreadData *)(arg);
  thread_data->thread_status = RUNNING;
  syslog(LOG_DEBUG, "Thread %ld started for client %d.", pthread_self(),
         thread_data->client_fd);

  if (receive_and_write_data(RESULT_FILE, thread_data->client_fd) == -1) {
    syslog(LOG_ERR, "receive_data");
    close(thread_data->client_fd);
    thread_data->thread_status = FAILED;
    pthread_exit(NULL);
  }

  // Send the contents of the file back to the client
  if (send_file(RESULT_FILE, thread_data->client_fd) == -1) {
    syslog(LOG_ERR, "send_file");
    close(thread_data->client_fd);
    thread_data->thread_status = FAILED;
    pthread_exit(NULL);
  }

  close(thread_data->client_fd);
  thread_data->thread_status = SUCCEEDED;
  pthread_exit(NULL);
}

void add_timespec(const struct timespec *lhs, const struct timespec *rhs,
                  struct timespec *result) {
  result->tv_sec = lhs->tv_sec + rhs->tv_sec;
  result->tv_nsec = lhs->tv_nsec + rhs->tv_nsec;

  // If nanoseconds exceed 1 second (1,000,000,000), adjust
  if (result->tv_nsec >= 1000000000L) {
    result->tv_sec += 1;
    result->tv_nsec -= 1000000000L;
  }
}

bool is_timespec_elapsed(const struct timespec *end_time,
                         const struct timespec *current_time) {
  if (current_time->tv_sec < end_time->tv_sec) {
    return false;
  }

  if (current_time->tv_sec > end_time->tv_sec) {
    return true;
  }

  // If we reach here, tv_sec is the same for both arguments
  if (current_time->tv_nsec <= end_time->tv_nsec) {
    return false;
  }

  // The current_time tv_sec and tv_nsec must be greater than the end_time
  // tv_sec and tv_nsec
  return true;
}

bool join_completed_threads(struct head_s *head) {
  bool is_error = false;

  struct node *slist_node = NULL;
  SLIST_FOREACH(slist_node, head, nodes) {
    syslog(LOG_DEBUG, "Thread: %ld  Status: %d\r\n", slist_node->thread,
           slist_node->thread_data.thread_status);

    switch (slist_node->thread_data.thread_status) {
    case UNITITIALIZED:
      break;
    case RUNNING:
      break;
    case FAILED:
      is_error = true;
      syslog(LOG_ERR, "Thread %ld reported an error", slist_node->thread);
    case SUCCEEDED: {
      const int pthread_join_result = pthread_join(slist_node->thread, NULL);
      if (pthread_join_result != 0) {
        is_error = true;
        syslog(LOG_ERR, "pthread_join returned error: %d", pthread_join_result);
      } else {
        syslog(LOG_DEBUG, "Thread joined for client %d.",
               slist_node->thread_data.client_fd);
      }

      SLIST_REMOVE(head, slist_node, node, nodes);
      free(slist_node);
      break;
    }
    default:
      syslog(LOG_ERR, "Unknown ThreadStatus");
      break;
    }
  }

  return is_error;
}

bool join_threads(struct head_s *head) {
  bool is_error = false;

  struct node *slist_node = NULL;
  SLIST_FOREACH(slist_node, head, nodes) {
    const int pthread_join_result = pthread_join(slist_node->thread, NULL);
    if (pthread_join_result != 0) {
      is_error = true;
      syslog(LOG_ERR, "pthread_join returned error: %d", pthread_join_result);
    } else {
      syslog(LOG_DEBUG, "Thread joined for client %d.",
             slist_node->thread_data.client_fd);
    }

    SLIST_REMOVE(head, slist_node, node, nodes);
    free(slist_node);
  }

  return is_error;
}