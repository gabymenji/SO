#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include "system_ipc.h"
#include "patient.h"
#include "log.h"


int shm_id = -1;
Statistics *stats = NULL;

int msq_id = -1;


long long now_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        return -1;
    }

    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

void create_named_pipe(){
    if (mkfifo(PIPE_NAME, 0666) == -1){
        if (errno != EEXIST){
            log_message("[IPC] ERROR mkfifo: %s", strerror(errno));
            exit(1);
        }
    }
    log_message("[IPC] Named pipe created: %s", PIPE_NAME);
}


void create_shared_memory(){
    shm_id = shmget(SHM_KEY, sizeof(Statistics), IPC_CREAT | 0666);
    if (shm_id <0){
        log_message("[IPC] ERROR shmget: %s", strerror(errno));
        exit(1);
    }

    stats = (Statistics *) shmat(shm_id, NULL, 0);
    if (stats == (void *) -1){
        log_message("[IPC] ERROR shmat: %s", strerror(errno));
        exit(1);
    }

    // 1: pshared (1 = partilhado entre processos), 0: valor inicial
    if (sem_init(&stats->shm_mutex, 1, 1) == -1) {
        log_message("[IPC] ERROR sem_init for SHM mutex: %s", strerror(errno));
        detach_shared_memory();
        exit(1);
    }

    stats->triaged = 0;
	stats->attended = 0;
	stats->total_wait_before_triage_ms = 0;
	stats->total_wait_after_triage_ms = 0;
	stats->total_system_time_ms = 0;

    log_message("[IPC] Shared memory created with ID: %d", shm_id);
}

void create_message_queue(){
    msq_id = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msq_id <0){
        log_message("[IPC] ERROR msgget: %s", strerror(errno));
        exit(1);
    }
    log_message("[IPC] Message queue created with ID: %d", msq_id);
}

void detach_shared_memory(){
    if (stats != NULL){
        shmdt(stats);
        stats = NULL;
    }
}

void shm_lock() {
    if (stats != NULL) {
        if (sem_wait(&stats->shm_mutex) == -1) {
            if (errno != EINTR) {
                log_message("[IPC] ERROR acquiring SHM lock: %s", strerror(errno));
            }
        }
    }
}

void shm_unlock() {
    if (stats != NULL) {
        if (sem_post(&stats->shm_mutex) == -1) {
            log_message("[IPC] ERROR releasing SHM lock: %s", strerror(errno));
        }
    }
}

void cleanup_ipc(){
    log_message("[IPC] Cleaning up IPC resources...");

    if (stats != NULL) {
        if (sem_destroy(&stats->shm_mutex) == -1) {
            log_message("[IPC] ERROR destroying SHM mutex: %s", strerror(errno));
        }
    }

    detach_shared_memory();

    if (shm_id != -1){
        if (shmctl(shm_id, IPC_RMID, NULL) == -1){
            log_message("[IPC] ERROR shmctl (removing SHM): %s", strerror(errno));
        } else {
            log_message("[IPC] Shared memory segment removed.");
        }
    }

    if (msq_id != -1){
        if (msgctl(msq_id, IPC_RMID, NULL) == -1){
            log_message("[IPC] ERROR msgctl (removing MSQ): %s", strerror(errno));
        } else {
            log_message("[IPC] Message queue removed.");
        }
    }

    unlink(PIPE_NAME);
    log_message("[IPC] Named pipe removed: %s", PIPE_NAME);
    log_message("[IPC] IPC cleanup complete.");
}

void display_statistics() {

    shm_lock();

    if (stats == NULL) {
        log_message("WARNING: Cannot display statistics. Shared Memory (SHM) not initialized.");
        shm_unlock();
        return;
    }

    // Ler e exibir as estatísticas
    log_message("--- EMERGENCY SYSTEM STATISTICS ---");
    log_message("Patients Triaged: %d", stats->triaged);
    log_message("Patients Attended: %d", stats->attended);

    if (stats->triaged > 0) {
        log_message("Average waiting time before triage: %.2f ms",
                    (double)stats->total_wait_before_triage_ms / stats->triaged);
    } else {
        log_message("Average waiting time before triage: unavailable (no patients triaged).");
    }

    if (stats->attended > 0) {
        log_message("Average waiting time between triage and attendance: %.2f ms",
                    (double)stats->total_wait_after_triage_ms / stats->attended);

        log_message("Average total time in system: %.2f ms",
                    (double)stats->total_system_time_ms / stats->attended);
    } else {
        log_message("Average waiting time between triage and attendance: unavailable (no patients attended).");
        log_message("Average total time in system: unavailable (no patients attended).");
    }
    log_message("-----------------------------------");

    shm_unlock();
}