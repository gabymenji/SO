#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "doctor.h"
#include "system_ipc.h"
#include "patient.h"
#include "log.h"

typedef struct {
    long mtype;
    Patient patient;
} MsgBuf;


void doctor_process(int id, int is_temporary, int shift_length) {

    long long start_time_ms = now_ms();

    log_message("[DOCTOR %d] Started (PID=%d). Type: %s. Shift Time: %d s.", id, getpid(), is_temporary ? "TEMPORARY" : "INITIAL", shift_length);

    MsgBuf msg;

    // Se as prioridades estiverem entre 1 e 3 (Prio mais alta=1), -3 garante a mais alta.
    long priority_selector = -(long)3;

	// Se for normal, block até receber uma mensagem
    int msgrcv_flags = is_temporary ? IPC_NOWAIT : 0;

    while (1) {

        // Terminação por Duração do Turno
        if (shift_length > 0 && (now_ms() - start_time_ms) >= shift_length * 1000LL) {
            log_message("[DOCTOR %d] Shift ended (%d seconds). Terminating.", id, shift_length);
            break;
        }

        if (msgrcv(msq_id, &msg, sizeof(Patient), priority_selector, msgrcv_flags) == -1) {

            // Terminação para Doctor TEMPORÁRIO quando a fila está vazia
            if (is_temporary && errno == ENOMSG) {
                log_message("[DOCTOR %d] Temporary doctor finished processing backlog (MSQ empty). Terminating.", id);
                break;
            }

            // Terminação por Fila Removida
            if (errno == EIDRM) {
                log_message("[DOCTOR %d] MSQ removed. Terminating.", id);
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            log_message("[DOCTOR %d] ERROR msgrcv: %s", id, strerror(errno));
            exit(1);
        }

        Patient p = msg.patient;
        p.attend_start_time_ms = now_ms();

		long long wait_after_triage_ms = p.attend_start_time_ms - p.triage_end_time_ms;

        log_message("[DOCTOR %d] START Attending. Patient %s (ID: %d, Prio: %d, Time: %d ms)", id, p.name, p.num_arrival, p.priority, p.attend_time);

        // Simulate attending the patient
        usleep(p.attend_time * 1000);

        p.attend_end_time_ms = now_ms();

		long long system_time_ms = p.attend_end_time_ms - p.arrival_time_ms;

        // Cálculo de tempo e atualização de stats
        long long attend_duration_ms = p.attend_end_time_ms - p.attend_start_time_ms;

        shm_lock();

        if (stats != NULL) {
            stats->attended++;
            stats->total_wait_after_triage_ms += wait_after_triage_ms;
    		stats->total_system_time_ms += system_time_ms;
        }

        shm_unlock();

        log_message("[DOCTOR %d] Finished %s. Attend duration: %lld ms. Total system time: %lld ms.", id, p.name, attend_duration_ms, system_time_ms);
    }
}

void spawn_doctors(int n, int shift_length){
    for (int i=0; i < n; i++){
        pid_t pid = fork();

        if (pid < 0){
            log_message("[DOCTOR] ERROR fork: %s", strerror(errno));
            exit(1);
        }

        if (pid == 0){
   			// Processo filho
            doctor_process(i, 0, shift_length);
            _exit(0);
        }
		// Processo pai
        log_message("[DOCTOR] Created initial doctor %d with PID %d", i, pid);
    }
}