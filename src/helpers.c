#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/time.h>

#include "constants.h"
#include "helpers.h"

void set_timer(int duration) {
	struct itimerval val;
	val.it_interval.tv_sec = duration;
	val.it_interval.tv_usec = 0;
	val.it_value = val.it_interval;
	if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
}

void crash(const char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;
	
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	
	perror(buf);
	
	exit(EXIT_FAILURE);
}

key_t key_from_id(int id) {
	key_t key = ftok(IPC_KEY_PATHNAME, id);
	if (key == -1) crash("ftok");
	return key;
}
