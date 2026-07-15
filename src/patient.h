typedef struct{
    char name[100];
    int triage_time;
    int attend_time;
    int priority;
    int num_arrival;
    long long arrival_time_ms;
	long long triage_start_time_ms;
	long long triage_end_time_ms;
	long long attend_start_time_ms;
	long long attend_end_time_ms;

} Patient;