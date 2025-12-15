#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include "doctor.h"
#include "system_ipc.h"
#include "patient.h"
#include "log.h"

typedef struct {
    long mtype;
    Patient patient;
} MsgBuf;

void doctor_process(int id) {
    log_message("[DOCTOR %d] Started (PID=%d)", id, getpid());

    MsgBuf msg;

    while (1) {

        if (msgrcv(msq_id, &msg, sizeof(Patient), 0, 0) == -1) {
            log_message("[DOCTOR %d] ERROR msgrcv: %s", id, strerror(errno));
            exit(1);
        }

        Patient p = msg.patient;

        log_message("[DOCTOR %d] START Attending. Patient %s (Nº Chegada: %d, Prioridade: %d, Tempo Atendimento: %d ms)", id, p.name, p.num_arrival, p.priority, p.attend_time);

        // Simulate attending the patient
        usleep(p.attend_time * 1000);


        if (stats != NULL) {
            stats->attended++;
        }

        log_message("[DOCTOR %d] Finished %s", id, p.name);
    }
}

void spawn_doctors(int n){
    for (int i=0; i < n; i++){
        pid_t pid = fork();

        if (pid < 0){
            log_message("[DOCTOR] ERROR fork: %s", strerror(errno));
            exit(1);
        }

        if (pid == 0){
            doctor_process(i);
            _exit(0);

        }
        log_message("[DOCTOR] Created doctor %d with PID %d", i, pid);
    }
}