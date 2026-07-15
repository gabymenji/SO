#include <semaphore.h>

typedef struct {
    sem_t shm_mutex;

    int triaged;
    int attended;
    long long total_wait_before_triage_ms;
    long long total_wait_after_triage_ms;
    long long total_system_time_ms;

} Statistics;