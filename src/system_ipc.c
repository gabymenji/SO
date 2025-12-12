#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>
#include "system_ipc.h"
#include "patient.h"

int shm_id = -1;
Statistics *stats = NULL;

int msq_id = -1;

void create_named_pipe(){
    if (mkfifo(PIPE_NAME, 0666) == -1){
        if (errno != EEXIST){
            perror("mkfifo");
            exit(1);
        }
    }
    printf("[IPC] Named pipe created: %s\n", PIPE_NAME);
}


void create_shared_memory(){
    shm_id = shmget(SHM_KEY, sizeof(Statistics), IPC_CREAT | 0666);
    if (shm_id <0){
        perror("[IPC] shmget");
        exit(1);
    }

    stats = (Statistics *) shmat(shm_id, NULL, 0);
    if (stats == (void *) -1){
        perror("[IPC] shmat");
        exit(1);
    }

    stats->triaged = 0;
    stats->attended = 0;

    printf("[IPC] Shared memory created with ID: %d\n", shm_id);
}

void create_message_queue(){
    msq_id = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msq_id <0){
        perror("[IPC] msgget");
        exit(1);
    }
    printf("[IPC] Message queue created with ID: %d\n", msq_id);
}

void detach_shared_memory(){
    if (stats != NULL){
        shmdt(stats);
        stats = NULL;
    }
}

void cleanup_ipc(){
    printf("[IPC] Cleaning up IPC resources...\n");

    detach_shared_memory();

    if (shm_id != -1){
        if (shmctl(shm_id, IPC_RMID, NULL) == -1){
            perror("[IPC] shmctl");
        } else {
            printf("[IPC] Shared memory segment removed.\n");
        }
    }

    if (msq_id != -1){
        if (msgctl(msq_id, IPC_RMID, NULL) == -1){
            perror("[IPC] msgctl");
        } else {
            printf("[IPC] Message queue removed.\n");
        }
    }

    unlink(PIPE_NAME);
    printf("[IPC] Named pipe removed: %s\n", PIPE_NAME);
    printf("[IPC] IPC cleanup complete.\n");
}
