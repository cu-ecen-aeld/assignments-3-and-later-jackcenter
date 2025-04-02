#include "utilities.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "../../aesd-char-driver/aesd_ioctl.h"

#include "config.h"
#include "queue.h"

int append_to_file(const int fd, char *buffer, const size_t buffer_len) {
  const ssize_t result = write(fd, buffer, buffer_len);
  if (result == -1) {
    perror("write");
    return -1;
  } else if (result != buffer_len) {
    syslog(LOG_ERR, "partial write");
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

void slist_free(struct head_s *head) {
  struct node *tmp;
  while (!SLIST_EMPTY(head)) {
    tmp = SLIST_FIRST(head);
    SLIST_REMOVE(head, tmp, node, nodes);
    free(tmp);
  }
}

bool slist_join_completed_threads(struct head_s *head) {
  bool is_error = false;

  struct node *slist_node = NULL;
  struct node *slist_next_node = NULL;
  SLIST_FOREACH_SAFE(slist_node, head, nodes, slist_next_node) {
    syslog(LOG_DEBUG, "Thread: %ld  Status: %d", slist_node->thread,
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
  struct node *slist_next_node = NULL;
  SLIST_FOREACH_SAFE(slist_node, head, nodes, slist_next_node) {
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

bool is_ioctl_command(const char *string) {
  // TODO: having this defined statically in two places (is_ioctl_command and
  // handle_ioctl_command_string) is not easy to maintain or scale. These should
  // be moved into the config if this is ever expanded on.
  static const char *command_string = "AESDCHAR_IOCSEEKTO:X, Y";
  static const size_t command_length =
      19; // chars in the `command_string` exlucding the variables `X` and `Y`

  // Checks first `command_length` chars in the `string to see if this is the
  // command we are looking for.
  return (strncmp(string, command_string, command_length) == 0);
}

bool get_ioctl_command_from_string(const char *string, struct aesd_seekto* seekto) {
  // TODO: having this defined statically in two places (is_ioctl_command and
  // handle_ioctl_command_string) is not easy to maintain or scale. These should
  // be moved into the config if this is ever expanded on.
  static const size_t command_length =
      19; // chars in the `command_string` exlucding the variables `X` and `Y`

  unsigned int X = 0;
  unsigned int Y = 0;
  int retval = 0;
  if ((retval = sscanf(string + command_length, "%u, %u", &X, &Y)) != 2) {
    syslog(LOG_ERR, "sscanf returned (%d) instead of the expected 2", retval);
    return false;
  }

  seekto->write_cmd = X;
  seekto->write_cmd_offset = Y;
  return true;
}
