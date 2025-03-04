#define PORT "9000"
#define BACKLOG (2)
#define RESULT_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE (1024)
#define TIMESTAMP_LOG_INTERVAL_S (10)

#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

pthread_mutex_t *config_get_result_file_mutex(void);

sem_t *config_get_timestamp_semaphore(void);

bool config_is_terminated(void);

void config_set_is_terminated(void);

