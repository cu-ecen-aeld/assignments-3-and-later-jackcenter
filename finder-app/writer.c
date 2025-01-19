#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("%d\r\n", argc);
    printf("Usage: %s [FILE] [TEXT]\r\n", argv[0]);
    return 1;
  }

  // Grab the args
  const char* file = argv[1];
  const char* data = argv[2];

  printf("File: %s, Data: %s\r\n", file, data);

  // Try to open
  const int file_descriptor = open(file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
  if (file_descriptor == -1) {
    const int err = errno;
    printf("Error: open - %d: %s\r\n", err, strerror(err));
    return -1;
  }

  // Write the data
  const size_t data_length = strlen(data);
  const ssize_t nr = write(file_descriptor, data, data_length);
  if (nr == -1) {
    const int err = errno;
    printf("Error: write - %d: %s\r\n", err, strerror(err));
    return -1;
  } else if (nr != data_length) {
    printf("Error: partial write\r\n");
    return -1;
  }


  printf("Fin\r\n");
  return 0;
}