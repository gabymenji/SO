#include <stdio.h>
#include <string.h>
#include "config.h"
#include "patient.h"

int read_config(Config *cfg){
    FILE *f = fopen("config.txt", "r");
    if (!f){
        perror("Failed to open config.txt");
        return -1;
    }

    char key[64];
    int val;

    while(fscanf(f, "%s = %d", key, &val) == 2){
        if (strcmp(key, "TRIAGE_QUEUE_MAX") == 0){
            cfg->TRIAGE_QUEUE_MAX = val;
        } else if (strcmp(key, "TRIAGE") == 0){
            cfg->TRIAGE = val;
        } else if (strcmp(key, "DOCTORS") == 0){
            cfg->DOCTORS = val;
        } else if (strcmp(key, "SHIFT_LENGTH") == 0){
            cfg->SHIFT_LENGTH = val;
        } else if (strcmp(key, "MSQ_WAIT_MAX") == 0){
            cfg->MSQ_WAIT_MAX = val;
        }
    }

    fclose(f);
    return 0;

}