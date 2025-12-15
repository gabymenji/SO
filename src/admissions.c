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

    printf("Configuration Loaded:\n");
    printf("TRIAGE_QUEUE_MAX: %d\n", cfg.TRIAGE_QUEUE_MAX);
    printf("TRIAGE: %d\n", cfg.TRIAGE);
    printf("DOCTORS: %d\n", cfg.DOCTORS);
    printf("SHIFT_LENGTH: %d\n", cfg.SHIFT_LENGTH);
    printf("MSQ_WAIT_MAX: %d\n", cfg.MSQ_WAIT_MAX);

    if (log_init() != 0){
        fprintf(stderr, "Error initializing log. Exiting.\n");
        return 1;
    }

	log_message("Admission Server started."); // Exemplo


    create_named_pipe();
    create_message_queue();
    create_shared_memory();

    if (queue_init(&triage_queue, cfg.TRIAGE_QUEUE_MAX) != 0){
        fprintf(stderr, "Error initializing triage queue.\n");
        return 1;
    }

    start_triage_threads(cfg.TRIAGE);

    spawn_doctors(cfg.DOCTORS);

    printf("Server running... Press Ctrl+C to stop.\n");

    int fd = open(PIPE_NAME, O_RDONLY);
    if (fd == -1){
        perror("open input_pipe");

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
                    printf("Pacient %s entered the triage queue.\n", pat.name);

                } else {
                    printf("Triage queue full. Patient %s descarted.\n", pat.name);
                }
            }

        }
    }

	log_message("Admission Server finishing.");
	log_cleanup(); // Limpar o MMF e fechar

}
