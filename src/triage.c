#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <errno.h>
#include "triage.h"
#include "system_ipc.h"
#include "log.h"

typedef struct{
    long mtype;
    Patient patient;
} MsgBuf;

TriageQueue triage_queue;
static pthread_t *triage_threads = NULL;
static int triage_thread_count = 0;
static int triage_running = 0;

int queue_init(TriageQueue *q, int max_size){
    q->buffer = (Patient *)malloc(sizeof(Patient) * max_size);
    if (!q->buffer) return -1;

    q->max_size = max_size;
    q->front = 0;
    q->rear = 0;
    q->count = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);

    return 0;
}

void queue_destroy(TriageQueue *q){
    if (!q) return;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q->buffer);
    q->buffer = NULL;
    q->max_size = q->front = q->rear = q->count = 0;
}

int queue_push(TriageQueue *q, const Patient *p){
    pthread_mutex_lock(&q->mutex);
    if (q->count == q->max_size){
        // Queue full
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    q->buffer[q->rear] = *p;
    q->rear = (q->rear + 1) % q->max_size;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;

}

int queue_pop(TriageQueue *q, Patient *out){
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && triage_running){
		// Fila vazia, espera até que haja um item
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }
    if (q->count == 0 && !triage_running){
        // Fila vazia e triagem encerrada
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    *out = q->buffer[q->front];
    q->front = (q->front + 1) % q->max_size;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return 0;

}

void *triage_worker(void *arg){
    int id = (int)(long)arg;
    log_message("[TRIAGE %d] Thread started.", id);

    Patient p;
    while(1){
        if (queue_pop(&triage_queue, &p) == 0){

            // Tempo de início da triagem
            p.triage_start_time_ms = now_ms();

            // Tempo de Espera (Wait Time)
            long long wait_before_triage_ms = p.triage_start_time_ms - p.arrival_time_ms;

            log_message("[TRIAGE %d] START Triaging patient %s (ID: %d, Wait: %ld s, Triage Time: %d ms).", id, p.name, p.num_arrival, wait_before_triage_ms, p.triage_time);

            // Simula o tempo de triagem (bloqueio da thread)
            usleep(p.triage_time * 1000);

			p.triage_end_time_ms = now_ms();

            // SINCRONIZAÇÃO E ATUALIZAÇÃO DE ESTATÍSTICAS
            shm_lock();

            if (stats != NULL){
                stats->triaged++;
				stats->total_wait_before_triage_ms += wait_before_triage_ms;
            }

            shm_unlock();

            // Enviar paciente para a Message Queue (MSQ)
            MsgBuf msg;
            msg.mtype = p.priority;
            msg.patient = p;

            if (msgsnd(msq_id, &msg, sizeof(Patient), 0) == -1) {
                log_message("[TRIAGE %d] ERROR msgsnd sending %s: %s", id, p.name, strerror(errno));
            } else {
                log_message("[TRIAGE %d] END Triaging. Patient %s sent to MSQ with Priority %d.", id, p.name, p.priority);
            }

        } else {
            // Se queue_pop retorna -1, estamos a terminar
            break;
        }
    }

    log_message("[TRIAGE %d] Thread exiting.", id);
    return NULL;
}

void start_triage_threads(int n){

    if ( n <= 0) return;
    triage_thread_count = n;
    triage_threads = malloc(sizeof(pthread_t) * n);
    triage_running = 1;

    for (int i = 0; i < n; i++){
        pthread_create(&triage_threads[i], NULL, triage_worker, (void *)(long)i);
        log_message("[TRIAGE] Created thread %d.", i);
    }

}

void stop_triage_threads(void){

    pthread_mutex_lock(&triage_queue.mutex);
    triage_running = 0;
    pthread_cond_broadcast(&triage_queue.not_empty); // wake up all
    pthread_mutex_unlock(&triage_queue.mutex);

    for (int i = 0; i < triage_thread_count; i++){
        pthread_join(triage_threads[i], NULL);
    }

    free(triage_threads);
    triage_threads = NULL;
    triage_thread_count = 0;
    queue_destroy(&triage_queue);

}