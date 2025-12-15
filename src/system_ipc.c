#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "system_ipc.h"
#include "patient.h"
#include "log.h"

int shm_id = -1;
Statistics *stats = NULL;

int msq_id = -1;

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

    stats->triaged = 0;
    stats->attended = 0;

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

void cleanup_ipc(){
    log_message("[IPC] Cleaning up IPC resources...");

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