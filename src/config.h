typedef struct{
    int TRIAGE_QUEUE_MAX;
    int TRIAGE;
    int DOCTORS;
    int SHIFT_LENGTH;
    int MSQ_WAIT_MAX;

}Config;

int read_config(Config *cfg);