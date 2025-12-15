#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>


char *log_map = NULL;
size_t log_current_pos = 0;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int log_fd = -1;

int log_init(){
    log_fd = open(LOG_FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (log_fd == -1) {
        perror("[LOG] Error opening/creating log file");
        return -1;
    }

    // Definir o tamanho do ficheiro
    if (ftruncate(log_fd, LOG_MAX_SIZE) == -1) {
        perror("[LOG] Error setting log file size");
        close(log_fd);
        return -1;
    }

    // Mapear o ficheiro em memória
    log_map = mmap(NULL, LOG_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, 0);
    if (log_map == MAP_FAILED) {
        perror("[LOG] Error mapping log file to memory");
        close(log_fd);
        return -1;
    }

    printf("[LOG] Log file mapped to memory successfully.\n");
    return 0;
}

/**
 * Escreve uma mensagem no ecrã e no MMF, precedida pela data/hora.
 * Deve ser chamado por todos os processos/threads.
 */
void log_message(const char *format, ...){
    pthread_mutex_lock(&log_mutex);

    // Obter a data/hora
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[64];
    // Formato de data/hora: [YYYY-MM-DD HH:MM:SS]
    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S] ", tm_info);

    // Formatar a mensagem original
    char message_buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    // Formatar a linha completa (Timestamp + Mensagem)
    char log_line[600];
    snprintf(log_line, sizeof(log_line), "%s%s\n", timestamp, message_buffer);

    // Escrever na Consola
    printf("%s", log_line);

    // Escrever no MMF (Memory-Mapped File)
    size_t len = strlen(log_line);
    if (log_current_pos + len < LOG_MAX_SIZE) {
        memcpy(log_map + log_current_pos, log_line, len);
        log_current_pos += len;
    } else {
        // Log Full - o projeto pede para dimensionar o suficiente.
        // Se isto acontecer, deve escrever um erro, mas o código prossegue.
        fprintf(stderr, "[LOG ERROR] Log file full. Message discarded: %s", log_line);
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_cleanup(){
    if (log_map != MAP_FAILED && log_map != NULL) {
        // Sincroniza as alterações do MMF com o disco
        if (msync(log_map, LOG_MAX_SIZE, MS_SYNC) == -1){
            perror("[LOG] msync error");
        }

        munmap(log_map, LOG_MAX_SIZE);
        log_map = NULL;
        printf("[LOG] Log memory unmapped.\n");
    }

    if (log_fd != -1) {
        close(log_fd);
        log_fd = -1;
        printf("[LOG] Log file closed.\n");
    }

    pthread_mutex_destroy(&log_mutex);
}