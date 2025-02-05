#include "threading.h"
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *) thread_param;
    
    usleep(1e3 * thread_func_args->wait_to_obtain_ms);
    pthread_mutex_lock(thread_func_args->mutex);
    usleep(1e3 * thread_func_args->wait_to_release_ms);
    pthread_mutex_unlock(thread_func_args->mutex);

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    // Allocate memory
    struct thread_data *thread_data_ptr = (struct thread_data *)malloc(sizeof(struct thread_data));
    thread_data_ptr->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data_ptr->wait_to_release_ms = wait_to_release_ms;
    thread_data_ptr->mutex = mutex;
    thread_data_ptr->thread_complete_success = false;

    // Create thread
    if (pthread_create(thread, NULL, threadfunc, (void*)thread_data_ptr) == 0) {
        thread_data_ptr->thread_complete_success = true;
    }

    return thread_data_ptr->thread_complete_success;
}