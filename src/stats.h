typedef struct {
    sem_t shm_mutex;

    int triaged;
    int attended;
    long total_triage_time;
    long total_wait_time;
    long total_attend_time;

} Statistics;