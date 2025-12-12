#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <string.h>
#include "doctor.h"
#include "system_ipc.h"
#include "patient.h"

typedef struct {
    long mtype;
    Patient patient;
} MsgBuf;

void doctor_process(int id) {
    printf("[DOCTOR %d] Started (PID=%d)\n", id, getpid());

    MsgBuf msg;

    while (1) {

        if (msgrcv(msq_id, &msg, sizeof(Patient), 0, 0) == -1) {
            perror("[DOCTOR] msgrcv");
            exit(1);
        }

        Patient p = msg.patient;

        printf("[DOCTOR %d] Attending %s (%d ms)\n", id, p.name, p.attend_time);

        // Simulate attending the patient
        usleep(p.attend_time * 1000);


        if (stats != NULL) {
            stats->attended++;
        }

        printf("[DOCTOR %d] Finished %s\n", id, p.name);
    }
}

void spawn_doctors(int n){
    for (int i=0; i < n; i++){
        pid_t pid = fork();

        if (pid < 0){
            perror("[DOCTOR] fork");
            exit(1);
        }

        if (pid == 0){
            doctor_process(i);
            _exit(0);

        }
        printf("[DOCTOR] Created doctor %d with PID %d\n", i, pid);
    }
}