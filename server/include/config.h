#define USE_AESD_CHAR_DEVICE (1)

#if USE_AESD_CHAR_DEVICE == 1
#define RESULT_FILE "/dev/aesdchar"
#else
#define RESULT_FILE "/var/tmp/aesdsocketdata"
#endif

#define PORT "9000"
#define BACKLOG (2)
#define BUFFER_SIZE (1024)
#define TIMESTAMP_LOG_INTERVAL_S (10)

#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

pthread_mutex_t *config_get_result_file_mutex(void);

sem_t *config_get_timestamp_semaphore(void);

bool config_is_terminated(void);

void config_set_is_terminated(void);
