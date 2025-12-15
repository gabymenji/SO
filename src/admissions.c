#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "config.h"
#include "system_ipc.h"
#include "triage.h"
#include "doctor.h"
#include "log.h"



int main(){
    printf("==== Admission: Starting Server ====\n");

    Config cfg;
    if (read_config(&cfg) != 0){
        fprintf(stderr, "Error reading configuration. Exiting.\n");
        return 1;
    }

    if (log_init() != 0){
        fprintf(stderr, "Error initializing log. Exiting.\n");
        return 1;
    }

	log_message("Admission Server started successfully.");
	log_message("Configutation Loaded");
	log_message("TRIAGE_QUEUE_MAX: %d", cfg.TRIAGE_QUEUE_MAX);
	log_message("TRIAGE: %d", cfg.TRIAGE);
    log_message("DOCTORS: %d", cfg.DOCTORS);
	log_message("SHIFT_LENGTH: %d", cfg.SHIFT_LENGTH);
	log_message("MSQ_WAIT_MAX: %d", cfg.MSQ_WAIT_MAX);


    create_named_pipe();
    create_message_queue();
    create_shared_memory();

    if (queue_init(&triage_queue, cfg.TRIAGE_QUEUE_MAX) != 0){
		log_message("Error initializing triage queue.");
        log_cleanup();
        return 1;
    }

    start_triage_threads(cfg.TRIAGE);

    spawn_doctors(cfg.DOCTORS);

    log_message("Server running... Waiting for SIGINT to stop.");

    int fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1){
        log_message("ERROR opening input_pipe: %s", strerror(errno));
    } else {
        char line[256];
        FILE *pipe_fp = fdopen(fd, "r");
        int arrival_counter = 0;
        while (fgets(line, sizeof(line), pipe_fp) != NULL){
            Patient pat;

            char name[64];
            int t, a, pr;

            if (sscanf(line, "%s %d %d %d", name, &t, &a, &pr) == 4 ){
                strcpy( pat.name, name);
                pat.triage_time = t;
                pat.attend_time = a;
                pat.priority = pr;

                if (queue_push(&triage_queue, &pat) == 0){
                    log_message("Pacient %s entered the triage queue.", pat.name);

                } else {
                    log_message("Triage queue full. Patient %s discarded.", pat.name);
                }
            }

        }
    }

	log_message("Admission Server finishing.");
	log_cleanup(); // Limpar o MMF e fechar

}
