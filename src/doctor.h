#include <sys/types.h>


void spawn_doctors(int n, int shift_length);
void doctor_process(int id, int is_temporary, int shift_length);
pid_t spawn_single_doctor(int id, int shift_length);