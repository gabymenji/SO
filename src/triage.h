#include <pthread.h>
#include <stdlib.h>
#include "patient.h"

typedef struct{
    Patient *buffer;
    int max_size;
    int front;
    int rear;
    int count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;

} TriageQueue;

extern TriageQueue triage_queue;

int queue_init(TriageQueue *q, int max_size);
void queue_destroy(TriageQueue *q);
int queue_push(TriageQueue *q, const Patient *p); //0 ok, -1 se cheia
int queue_pop(TriageQueue *q, Patient *out); //bloqueante até ter item

void start_triage_threads(int n);
void stop_triage_threads(void);
