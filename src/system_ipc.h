#include "stats.h"

#define PIPE_NAME "input_pipe"

#define SHM_KEY IPC_PRIVATE
#define MSG_KEY 0x5678

extern int msq_id;
extern int shm_id;
extern Statistics *stats;

long long now_ms(void);
void create_named_pipe();
void create_shared_memory();
void create_message_queue();

void cleanup_ipc();
void detach_shared_memory();

void shm_lock();
void shm_unlock();
void display_statistics();
