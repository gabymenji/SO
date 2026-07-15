CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: admission

admission:
	$(CC) $(CFLAGS) src/admissions.c src/config.c src/system_ipc.c src/triage.c src/doctor.c src/log.c src/signals.c -o admission

clean:
	rm -f admission