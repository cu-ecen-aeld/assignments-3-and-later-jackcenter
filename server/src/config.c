#include "config.h"

#include <stdbool.h>
#include <semaphore.h>

bool is_terminated = false;
sem_t timestamp_semaphore;
pthread_mutex_t result_file_mutex;

pthread_mutex_t *config_get_result_file_mutex(void) {
  return &result_file_mutex;
}

sem_t *config_get_timestamp_semaphore(void) {
  return &timestamp_semaphore;
}

bool config_is_terminated(void) {
  return is_terminated;
}

void config_set_is_terminated(void) {
  is_terminated = true;
}
