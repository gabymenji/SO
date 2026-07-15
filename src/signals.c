#include "signals.h"
#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "system_ipc.h"
#include "triage.h"

// Variável global para controlar o loop principal (inicialmente 1 = running)
volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t sigusr1_received = 0;

void handle_sigint(int signum) {
    if (signum == SIGINT) {
        server_running = 0;
    }
}

void handle_sigusr1(int signum) {
    if (signum == SIGUSR1) {
        sigusr1_received = 1;

        // display_statistics();
    }
}

void setup_signal_handlers() {
    struct sigaction sa;

    // Configurar SIGINT (Terminação)
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        exit(1);
    }

    // Configurar SIGUSR1 (Estatísticas)
    sa.sa_handler = handle_sigusr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction SIGUSR1");
        exit(1);
    }

    // Opcional, mas recomendado:
    // signal(SIGPIPE, SIG_IGN);
}