/**
 * TODO: (low-priority) socket_client_receive_and_write_data could use a timeout
 * incase the client opens the connection but never sends any data
 * TODO: (high-priority) socket_client_receive_and_write_data should read data
 * into the heap and use realloc if the buffer is too small. By doing this the
 * file would only need to be openned for long enough to write that heap buffer
 * instead of being openned repeatedly with the stack buffer. This would also
 * result in the mutex being locked for a shorter duration. Right now if a
 * client opens a connection and doesn't send data, the mutex lock will block
 * all other threads indefinitley.
 */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
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

#include "config.h"
#include "queue.h"
#include "socket_client.h"
#include "socket_server.h"
#include "utilities.h"

typedef struct {
  int socket_server_fd;
  struct addrinfo **server_addrinfo;
  pthread_t timestamp_logger_thread;
} SetupData;

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
 * @brief Sets up the socket server.
 * @param execute_as_daemon true to execute as a daemon
 * @param server_socket pointer to storage location for socket server file
 * descriptor
 * @param server_addrinfo pointer to pointer of the servers addrinfo
 * @return 0 if successful
 * @return 1 if parent process that needs to be ended
 * @return -1 otherwise
 */
static int setup_socket_server(const bool execute_as_daemon, int *server_socket,
                               struct addrinfo **server_addrinfo);

/**
 * @brief Sets up the SIGINT, SIGTERM, and SIGUSR1 signal handlers.
 * @return 0 if successful
 * @return 1 otherwise
 */
static int setup_signal_handlers(void);

/**
 * @brief Intended to be run in a thread. Completes the data transfer from the
 * client passed in `arg`. Data from the client is recieved and written to file,
 * then the entire contents of the file are sent back to the client.
 * @param arg client file descriptor as an int*
 */
static void *data_transfer_worker(void *arg);

/**
 * @brief Writes a timestamp in RFC 2822 complient format to `RESULT_FILE`
 * triggered by the `timestamp_sem`. This is inteneded to be run in a thread.
 */
static void *log_timestamp_worker(void *arg);

/**
 * @brief SIGINT signal handler used to gracefully shutdown the program
 * @param sig signal number
 */
static void sigint_handler(int sig);

/**
 * @brief SIGUSR1 signal handler used to handle timestamp logging events
 * @param sig signal number
 */
static void sigusr1_handler(int sig);

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

  int server_socket = 0;
  struct addrinfo *server_addrinfo = NULL;
  if (setup_socket_server(execute_as_daemon, &server_socket,
                          &server_addrinfo)) {
    syslog(LOG_ERR, "setup_socket_server");
    close(server_socket);
    freeaddrinfo(server_addrinfo);
    closelog();
    return -1;
  }

  if (setup_signal_handlers()) {
    syslog(LOG_ERR, "setup_signal_handlers");
    close(server_socket);
    freeaddrinfo(server_addrinfo);
    closelog();
    return -1;
  }

  // Setup Timestamp Logger
  pthread_t timestamp_thread_id;
  const int pthread_create_result =
      pthread_create(&timestamp_thread_id, NULL, log_timestamp_worker, NULL);
  if (pthread_create_result) {
    syslog(LOG_ERR, "pthread_create returned error: %d", pthread_create_result);
    close(server_socket);
    freeaddrinfo(server_addrinfo);
    closelog();
    return -1;
  }

  // Initialize result file mutex
  if (pthread_mutex_init(config_get_result_file_mutex(), NULL)) {
    perror("pthread_mutex_init");
    close(server_socket);
    freeaddrinfo(server_addrinfo);
    closelog();
    return -1;
  }
  // Run the application
  const int result = application(server_socket);

  // Join the timestamp logger
  pthread_join(timestamp_thread_id, NULL);

  // Delete the file that is open during the application
  if (remove(RESULT_FILE) && (errno != ENOENT)) {
    perror("remove");
  }

  // Clean up
  pthread_mutex_destroy(config_get_result_file_mutex());
  close(server_socket);
  freeaddrinfo(server_addrinfo);
  closelog();
  return result;
}

int application(const int server_fd) {
  syslog(LOG_DEBUG, "Starting `aesdsocket`.");

  // Initialize a queue
  struct head_s head;
  SLIST_INIT(&head);

  // Loop until termination signal is received
  const struct timespec timeout = {.tv_sec = 1, .tv_nsec = 0};
  while (!config_is_terminated()) {
    // Wait for a connection
    int client_socket = 0;
    const int connection_result =
        socket_client_create_connection(server_fd, &client_socket, &timeout);
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
      if (!slist_join_completed_threads(&head)) {
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
    const int pthread_create_result =
        pthread_create(&thread_id, NULL, data_transfer_worker,
                       (void *)&(slist_data->thread_data));
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
  slist_join_threads(&head);
  return 0;
}

int setup_socket_server(const bool execute_as_daemon, int *server_socket,
                        struct addrinfo **server_addrinfo) {
  // Setup the socket server
  if (socket_server_create(server_socket, server_addrinfo) != 0) {
    syslog(LOG_ERR, "create_server_socket");
    return -1;
  }

  // Handle daemon execution if selected. Must be done after socket is setup
  if (execute_as_daemon) {
    const int daemon_resut = daemonize();
    switch (daemon_resut) {
    case -1:
      syslog(LOG_ERR, "daemonize");
      return -1;
    case 1:
      return 1;
    default:
    }
  }

  return 0;
}

int setup_signal_handlers(void) {
  // Register SIGINT handler
  if (signal(SIGINT, sigint_handler) == SIG_ERR) {
    perror("signal");
    return -1;
  }

  // Register SIGTERM handler
  if (signal(SIGTERM, sigint_handler) == SIG_ERR) {
    perror("signal");
    return -1;
  }

  // Initialize clock in the child (if daemonized) with a 10s loop
  struct sigaction sigusr1_sa;
  sigusr1_sa.sa_handler = sigusr1_handler;
  sigemptyset(&sigusr1_sa.sa_mask);
  sigaction(SIGUSR1, &sigusr1_sa, NULL);

  timer_t log_timestamp_timer;

  struct sigevent evp;
  evp.sigev_value.sival_ptr = &log_timestamp_timer;
  evp.sigev_notify = SIGEV_SIGNAL;
  evp.sigev_signo = SIGUSR1;
  if (timer_create(CLOCK_MONOTONIC, &evp, &log_timestamp_timer)) {
    perror("timer_create");
    return -1;
  }

  struct itimerspec timestamp_log_itimespec;
  timestamp_log_itimespec.it_interval.tv_sec = TIMESTAMP_LOG_INTERVAL_S;
  timestamp_log_itimespec.it_interval.tv_nsec = 0;
  timestamp_log_itimespec.it_value.tv_sec = TIMESTAMP_LOG_INTERVAL_S;
  timestamp_log_itimespec.it_value.tv_nsec = 0;

  if (timer_settime(log_timestamp_timer, 0, &timestamp_log_itimespec, NULL)) {
    perror("timer_settime");
    return -1;
  }

  if (sem_init(config_get_timestamp_semaphore(), 0 /* shared between threads */,
               0)) {
    perror("sem_init");
    return -1;
  }

  return 0;
}

void *data_transfer_worker(void *arg) {
  ThreadData *thread_data = (ThreadData *)(arg);
  thread_data->thread_status = RUNNING;
  syslog(LOG_DEBUG, "Thread %ld started for client %d.", pthread_self(),
         thread_data->client_fd);

  pthread_mutex_lock(config_get_result_file_mutex());
  if (socket_client_receive_and_write_data(RESULT_FILE,
                                           thread_data->client_fd) == -1) {
    pthread_mutex_unlock(config_get_result_file_mutex());
    syslog(LOG_ERR, "receive_data");
    close(thread_data->client_fd);
    thread_data->thread_status = FAILED;
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(config_get_result_file_mutex());

  // Send the contents of the file back to the client
  pthread_mutex_lock(config_get_result_file_mutex());
  if (socket_client_send_file(RESULT_FILE, thread_data->client_fd) == -1) {
    pthread_mutex_unlock(config_get_result_file_mutex());
    syslog(LOG_ERR, "send_file");
    close(thread_data->client_fd);
    thread_data->thread_status = FAILED;
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(config_get_result_file_mutex());

  close(thread_data->client_fd);
  thread_data->thread_status = SUCCEEDED;
  pthread_exit(NULL);
}

void *log_timestamp_worker(void *arg) {
  // Initial wait for first timestamp write
  sem_wait(config_get_timestamp_semaphore());
  while (!config_is_terminated()) {
    struct timespec log_timestamp;
    clock_gettime(CLOCK_REALTIME, &log_timestamp);

    struct tm timestamp_info;
    if (localtime_r(&log_timestamp.tv_sec, &timestamp_info) == NULL) {
      syslog(LOG_ERR, "localtime_r");
      continue;
    }

    // write time
    char time_string[128];
    if (strftime(time_string, sizeof(time_string),
                 "timestamp:%a, %d %b %Y %H:%M:%S %z", &timestamp_info) == 0) {
      syslog(LOG_ERR, "stfrtime");
      continue;
    }
    strcat(time_string, "\n");

    pthread_mutex_lock(config_get_result_file_mutex());
    if (append_to_file(RESULT_FILE, time_string, strlen(time_string))) {
      syslog(LOG_ERR, "append_to_file");
    }
    pthread_mutex_unlock(config_get_result_file_mutex());

    // Subsequent wait. This allows is_terminated to be set then the semaphore
    // to be posted to quickly rejoin this thread.
    sem_wait(config_get_timestamp_semaphore());
  }

  pthread_exit(NULL);
}

void sigint_handler(int sig) {
  syslog(LOG_DEBUG, "termination signal received");
  config_set_is_terminated();
  sem_post(config_get_timestamp_semaphore());
}

static void sigusr1_handler(int sig) {
  sem_post(config_get_timestamp_semaphore());
}