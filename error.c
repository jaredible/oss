#include "error.h"

#define BUFFER_LENGTH 1024

void error(char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	fprintf(stderr, "%s\n", buf);
}

void crash(char *msg) {
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%s", msg);
	perror(buf);
	exit(EXIT_FAILURE);
}
