#ifndef UTILITIES_H
#define UTILITIES_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "../../aesd-char-driver/aesd_ioctl.h"

#include "queue.h"

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

/**
 * @brief Appends the `buffer` to the `file` and creates the file if it doesn't
 * exist. Requires caller to pen file.
 * @param fd file descriptor
 * @param buffer data to write
 * @param buffer_len size of the data in `buffer`
 * @return 0 if successful
 * @return -1 otherwise
 */
int append_to_file(const int fd, char *buffer, const size_t buffer_len);

/**
 * @brief Sets up the daemon to run the program
 * @return 0 if successful (in daemon process)
 * @return 1 if in parent process
 * @return -1 otherwise
 */
int daemonize(void);

/**
 * @brief Gets a line from the `stream`
 * @param stream file stream
 * @param line pointer to the location of the line string
 * @param line_len pointer to location to stor the line length
 * @return 0 if successful
 * @return 1 if at the end of the file
 * @return -1 otherwise
 */
int read_line_from_stream(FILE *stream, char **line, size_t *line_len);

/**
 * @brief Frees all elements from the linked list `head`.abort
 */
void slist_free(struct head_s *head);

/**
 * @brief Joins any thread in the linked list `head` that is complete.
 * @return true if successful
 * @return false otherwise
 */
bool slist_join_completed_threads(struct head_s *head);

/**
 * @brief Waits for all threads in the linked list `head` to join.
 * @return true if successful
 * @return false otherwise
 */
bool slist_join_threads(struct head_s *head);

/**
 * @brief Adds the `lhs` and `rhs` timespecs together and puts them in `result`.
 */
void timespec_add(const struct timespec *lhs, const struct timespec *rhs,
                  struct timespec *result);

/**
 * @brief returns true if the `current_time` is greater than the `end_time`.
 */
bool timespec_is_elapsed(const struct timespec *end_time,
                         const struct timespec *current_time);

/**
 * @brief checks the `string` to determine if it is an ioctl command
 */
bool is_ioctl_command(const char *string);

/**
 * @brief creates an `aesd_seekto` command from the string. Should be checked
 * with is_ioctl_command() first.
 */
bool get_ioctl_command_from_string(const char *string,
                                   struct aesd_seekto *seekto);

#endif // UTILITIES_H
