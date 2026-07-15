#include <signal.h>

extern volatile sig_atomic_t server_running;
extern volatile sig_atomic_t sigusr1_received;

void setup_signal_handlers();
void handle_sigint(int signum);
void handle_sigusr1(int signum);
