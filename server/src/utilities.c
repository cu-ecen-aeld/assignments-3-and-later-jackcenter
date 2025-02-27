#include "utilities.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "queue.h"

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

bool slist_join_completed_threads(struct head_s *head) {
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

bool slist_join_threads(struct head_s *head) {
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

void timespec_add(const struct timespec *lhs, const struct timespec *rhs,
                  struct timespec *result) {
  result->tv_sec = lhs->tv_sec + rhs->tv_sec;
  result->tv_nsec = lhs->tv_nsec + rhs->tv_nsec;

  // If nanoseconds exceed 1 second (1,000,000,000), adjust
  if (result->tv_nsec >= 1000000000L) {
    result->tv_sec += 1;
    result->tv_nsec -= 1000000000L;
  }
}


bool timespec_is_elapsed(const struct timespec *end_time,
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
