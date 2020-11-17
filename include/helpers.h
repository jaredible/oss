#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include <sys/types.h>

#include "shared.h"

#define TRACE_LOG(fmt, args...) fprintf(stdout, fmt, ##args);

typedef struct {
	bool verbose;
} opts_t;

opts_t parse_cmd_args(int, char*[]);
void set_timer(int);
void crash(const char*, ...);
key_t key_from_id(int);

#endif
