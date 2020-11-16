#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "shared.h"

#define LOG_PATH "./output.log"

static char *programName = NULL;

void init(int argc, char **argv) {
	programName = argv[0];
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

char *getProgramName() {
	return programName;
}

void error(char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	fprintf(stderr, "%s: %s\n", getProgramName(), buf);
}

void crash(char *msg) {
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%s: %s", getProgramName(), msg);
	perror(buf);
	exit(EXIT_FAILURE);
}

void slog(char *fmt, ...) {
	FILE *fp = fopen(LOG_PATH, "a+");
	if (fp == NULL) crash("fopen");
	
	char argbuf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(argbuf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	
	char logbuf[BUFFER_LENGTH];
	snprintf(logbuf, BUFFER_LENGTH, "%s: %s\n", basename(getProgramName()), argbuf);
	
	//fprintf(fp, logbuf);
	printf(logbuf);
	
	if (fclose(fp) == EOF) crash("fclose");
}
