#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/msg.h>
#include <time.h>
#include "config.h"
#include "system_ipc.h"
#include "triage.h"
#include "doctor.h"
#include "log.h"
#include "signals.h"

#define MAX_DOCTORS 100

extern volatile sig_atomic_t server_running;
extern volatile sig_atomic_t sigusr1_received;

pid_t doctor_pids[MAX_DOCTORS];
int doctor_ids[MAX_DOCTORS];
int normal_doctor_count = 0;
int global_shift_length = 0;


void start_normal_doctors(int n, int shift_length) {
	if (n > MAX_DOCTORS) {
		log_message("[ADMISSION] ERROR: Too many doctors configured (%d). Max is %d.", n, MAX_DOCTORS);
		n = MAX_DOCTORS;
	}

	normal_doctor_count = n;
	global_shift_length = shift_length;

	for (int i = 0; i < n; i++) {
		doctor_ids[i] = i;
		doctor_pids[i] = spawn_single_doctor(i, shift_length);

		if (doctor_pids[i] == -1) {
			log_message("[ADMISSION] ERROR creating normal Doctor %d.", i);
		}
	}
}


void wait_for_doctors(void) {

    int status;
    pid_t wpid;

    log_message("Waiting for Doctor processes to finish...");

    // pode-se enviar SIGTERM aos Doctors que não acabarem o paciente a tempo, mas por agora, apenas esperamos.
    while ((wpid = waitpid(-1, &status, 0)) > 0) {
        if (WIFEXITED(status)) {
            log_message("Doctor process %d terminated normally (status=%d).", wpid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            log_message("Doctor process %d terminated by signal %d.", wpid, WTERMSIG(status));
        }
    }

    if (wpid == -1 && errno != ECHILD) {
        log_message("ERROR during waitpid: %s", strerror(errno));
    }
    log_message("All initial Doctor processes have been collected.");
}

void wait_for_finished_doctors() {
	int status;
	pid_t wpid;

	while ((wpid = waitpid(-1, &status, WNOHANG)) > 0) {

		int was_normal_doctor = 0;
		int doctor_index = -1;

		for (int i = 0; i < normal_doctor_count; i++) {
			if (doctor_pids[i] == wpid) {
				was_normal_doctor = 1;
				doctor_index = i;
				break;
			}
		}

		if (WIFEXITED(status)) {
			log_message("[ADMISSION] Collected Doctor process %d (status=%d).",
						wpid, WEXITSTATUS(status));
		} else if (WIFSIGNALED(status)) {
			log_message("[ADMISSION] Collected Doctor process %d terminated by signal %d.",
						wpid, WTERMSIG(status));
		}

		if (was_normal_doctor && server_running) {
			int doctor_id = doctor_ids[doctor_index];

			log_message("[ADMISSION] Normal Doctor %d ended shift. Creating replacement.",
						doctor_id);

			pid_t new_pid = spawn_single_doctor(doctor_id, global_shift_length);

			if (new_pid != -1) {
				doctor_pids[doctor_index] = new_pid;
				log_message("[ADMISSION] Replacement Doctor %d created with PID %d.",
							doctor_id, new_pid);
			} else {
				doctor_pids[doctor_index] = -1;
				log_message("[ADMISSION] ERROR creating replacement for Doctor %d.",
							doctor_id);
			}
		}
	}
}

// Assume que doctor_process(int id) existe e o doctor sabe como morrer (ponto 4).
void spawn_temporary_doctor(int id, int shift_length) {
    pid_t pid = fork();
    if (pid < 0) {
        log_message("[ADMISSION] ERROR fork for TEMP DOCTOR: %s", strerror(errno));
        return;
    }
    if (pid == 0) {
        // Processo Filho (Doctor)
        doctor_process(id, 1, shift_length);
        _exit(0);
    }
    log_message("[ADMISSION] Created TEMPORARY Doctor (ID: %d, PID: %d) due to high MSQ load.", id, pid);
    // Não precisamos de armazenar o PID, ele será recolhido por wait_for_finished_doctors.
}


void remove_message_queue_for_shutdown() {
	if (msq_id != -1) {
		if (msgctl(msq_id, IPC_RMID, NULL) == -1) {
			log_message("[ADMISSION] ERROR removing MSQ during shutdown: %s", strerror(errno));
		} else {
			log_message("[ADMISSION] Message Queue removed to unblock Doctors.");
			msq_id = -1;
		}
	}
}


void wait_until_msq_empty() {
	struct msqid_ds msq_info;

	log_message("[ADMISSION] Waiting for Message Queue to become empty...");

	while (1) {
		if (msgctl(msq_id, IPC_STAT, &msq_info) == -1) {
			if (errno == EIDRM || errno == EINVAL) {
				log_message("[ADMISSION] MSQ already removed.");
				break;
			}

			log_message("[ADMISSION] ERROR reading MSQ status during shutdown: %s", strerror(errno));
			break;
		}

		if (msq_info.msg_qnum == 0) {
			log_message("[ADMISSION] Message Queue is empty.");
			break;
		}

		log_message("[ADMISSION] Waiting... MSQ still has %lu patients.", msq_info.msg_qnum);

		wait_for_finished_doctors();

		usleep(100000);
	}
}


int main(){
    printf("==== Admission: Starting Server ====\n");

    Config cfg;
    if (read_config(&cfg) != 0){
        fprintf(stderr, "Error reading configuration. Exiting.\n");
        return 1;
    }

	//Cria e mapeia o ficheiro log
    if (log_init() != 0){
        fprintf(stderr, "Error initializing log. Exiting.\n");
        return 1;
    }

    log_message("Admission Server started successfully.");
    log_message("Configuration Loaded");
    log_message("TRIAGE_QUEUE_MAX: %d", cfg.TRIAGE_QUEUE_MAX);
    log_message("TRIAGE: %d", cfg.TRIAGE);
    log_message("DOCTORS: %d", cfg.DOCTORS);
    log_message("SHIFT_LENGTH: %d", cfg.SHIFT_LENGTH);
    log_message("MSQ_WAIT_MAX: %d", cfg.MSQ_WAIT_MAX);

	//Criação dos recursos do sistema
    setup_signal_handlers();
    create_named_pipe();
    create_message_queue();
    create_shared_memory();

    if (queue_init(&triage_queue, cfg.TRIAGE_QUEUE_MAX) != 0){
       log_message("Error initializing triage queue.");
        log_cleanup();
        return 1;
    }

	//Criação das threads e dos Doctors
    start_triage_threads(cfg.TRIAGE);
	start_normal_doctors(cfg.DOCTORS, cfg.SHIFT_LENGTH);

    log_message("Server running... Waiting for SIGINT to stop.");

	//Número sequencial para cada paciente
	int arrival_counter = 0;
	//Para atribuir a Doctors temporários, depois dos Doctors normais
	int next_temp_doctor_id = cfg.DOCTORS;

	struct msqid_ds msq_info;

	// Abre o input_pipe para leitura
    int fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1){
        log_message("ERROR opening input_pipe: %s", strerror(errno));
        log_cleanup();
        return 1;
    }

    char line[256];

	// Enquanto o servidor continua ativo
    while (server_running) {

    	if (sigusr1_received) {
    		sigusr1_received = 0;
    		log_message("SIGUSR1 received. Printing statistics...");
    		display_statistics();
    	}

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);

        // max_fd é o maior descritor de ficheiro + 1.
        int max_fd = fd + 1;

		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

        // select() espera 1 segundo ou por I/O/sinal
        int activity = select(max_fd, &readfds, NULL, NULL, &timeout); // Para n ficar bloqueado permanentemente no read()
		wait_for_finished_doctors();
        if (activity == -1) {
            if (errno == EINTR) {
                continue;
            }
            // Erro crítico.
            log_message("ERROR during select(): %s", strerror(errno));
            server_running = 0;
            continue;
        }

		if (activity == 0) {
            // Se o timeout expirou (atividade=0), verificar a MSQ.
            if (msgctl(msq_id, IPC_STAT, &msq_info) != -1) {
                // Se o número de mensagens na MSQ exceder MSQ_WAIT_MAX, criar um Doctor temporário.
                if (msq_info.msg_qnum > (unsigned long)cfg.MSQ_WAIT_MAX) {

                    log_message("[ADMISSION] HIGH LOAD: MSQ count (%lu) > WAIT_MAX (%d). Spawning temporary doctor.",msq_info.msg_qnum, cfg.MSQ_WAIT_MAX);

                    // Criar um Doctor temporário
                    spawn_temporary_doctor(next_temp_doctor_id++, cfg.SHIFT_LENGTH);
                }
            } else {
                log_message("[ADMISSION] ERROR reading MSQ status: %s", strerror(errno));
            }
        }

        if (activity > 0) {
            if (FD_ISSET(fd, &readfds)) {
				// Leitura de pacientes do input_pipe
                ssize_t bytes_read = read(fd, line, sizeof(line) - 1);

                if (bytes_read > 0) {

                    line[bytes_read] = '\0';

                    int t, a, pr; // Tempo de triagem, atendimento e prioridade
					int num_patients;
					int success = 0;

                    if (sscanf(line, "%d %d %d %d", &num_patients, &t, &a, &pr) == 4 && num_patients > 0){
						log_message("Received Group Command: %d patients (T: %d, A: %d, P: %d).", num_patients, t, a, pr);

						long long arrival_time_ms = now_ms(); // O grupo chega no mesmo instante
						int current_group_base_id = arrival_counter + 1;

                		// Criar e inserir cada paciente do grupo
                		for (int i = 1; i <= num_patients; i++) {
                    		Patient pat;
                    		char group_name[64];

                    		// Gerar nome sequencial (ex: GRP_12_1, GRP_12_2)
                    		snprintf(group_name, sizeof(group_name), "GRP_%d_%d", current_group_base_id, i);

                    		strcpy(pat.name, group_name);
                    		pat.triage_time = t;
                    		pat.attend_time = a;
                    		pat.priority = pr;

                    		// Incremento para cada paciente criado
                    		pat.num_arrival = ++arrival_counter;
                    		pat.arrival_time_ms = arrival_time_ms;
                			pat.triage_start_time_ms = 0;
                			pat.triage_end_time_ms = 0;
                			pat.attend_start_time_ms = 0;
                			pat.attend_end_time_ms = 0;

                    		if (queue_push(&triage_queue, &pat) == 0){
                        		log_message("Pacient %s (Group) entered the triage queue (ID: %d).", pat.name, pat.num_arrival);
                    		} else {
                        		log_message("Triage queue full. Patient %s (Group) discarded.", pat.name);
                    		}
						}
						success = 1;
					}
					else {
						char name[64];
						if (sscanf(line, "%s %d %d %d", name, &t, &a, &pr) == 4){
                        	Patient pat;

                        	strcpy( pat.name, name);
                        	pat.triage_time = t;
                        	pat.attend_time = a;
                        	pat.priority = pr;

							pat.num_arrival = ++arrival_counter;

                    		pat.arrival_time_ms = now_ms();
							pat.triage_start_time_ms = 0;
							pat.triage_end_time_ms = 0;
							pat.attend_start_time_ms = 0;
							pat.attend_end_time_ms = 0;

                        	if (queue_push(&triage_queue, &pat) == 0){
                            	log_message("Pacient %s entered the triage queue.", pat.name);
                        	} else {
                            	log_message("Triage queue full. Patient %s discarded.", pat.name);
                        	}
							success = 1;
                    	}
					}
					if (!success) {
                         log_message("ERROR: Invalid input format received from pipe. Line ignored: '%s'", line);
                    }
                } else if (bytes_read == 0) {
                    log_message("Input pipe closed by writer. Stopping server.");
                    server_running = 0;
                } else if (bytes_read == -1 && errno != EINTR) {
                    log_message("ERROR reading from input_pipe: %s", strerror(errno));
                }
            }
        }
    }

	log_message("SIGINT received. Starting graceful shutdown.");
	log_message("Admission Server stopped accepting new patients.");

    close(fd); // Fechar o input_pipe
    stop_triage_threads();
	log_message("[ADMISSION] All triage threads finished.");
	wait_until_msq_empty();
	remove_message_queue_for_shutdown();
    wait_for_doctors();
    cleanup_ipc();

    log_message("Admission Server terminated successfully.");
    log_cleanup();

    return 0;

}