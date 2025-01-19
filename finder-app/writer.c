#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  openlog("writer", LOG_PID, LOG_USER);
  syslog(LOG_DEBUG, "Starting `writer`.");

  if (argc != 3) {
    printf("%d\r\n", argc);
    printf("Usage: %s [FILE] [TEXT]\r\n", argv[0]);
    return 1;
  }

  // Parse the arguments
  const char* file = argv[1];
  const char* data = argv[2];

  // Open the file
  const int file_descriptor = open(file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
  if (file_descriptor == -1) {
    const int err = errno;
    syslog(LOG_ERR, "%d: %s", err, strerror(err));
    return -1;
  }

  // Write the data
  const size_t data_length = strlen(data);
  syslog(LOG_DEBUG, "Writing %s to %s.", data, file);
  const ssize_t nr = write(file_descriptor, data, data_length);
  if (nr == -1) {
    const int err = errno;
    syslog(LOG_ERR, "%d: %s", err, strerror(err));
    return -1;
  } else if (nr != data_length) {
    syslog(LOG_ERR, "Error: partial write");
    return -1;
  }

  syslog(LOG_DEBUG, "Completing `writer`.");
  closelog();
  return 0;
}