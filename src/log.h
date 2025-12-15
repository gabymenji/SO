#include <stdio.h>
#include <pthread.h>

#define LOG_FILENAME "DEI_Emergency.log"

#define LOG_MAX_SIZE (10 * 1024 * 1024) // Ex: 10 MB

extern char *log_map;
extern size_t log_current_pos;
extern pthread_mutex_t log_mutex;

int log_init();
void log_message(const char *format, ...);
void log_cleanup();
