#ifndef OSS_H
#define OSS_H

#include "clock.h"
#include "shared.h"

void register_signal_handlers();
void handle_interrupt(int);
void handle_alarm(int);
void terminate_children();
void await_children();
void cleanup();

void oss();
void try_spawn();
void spawn();
void check_messages();
void handle_message();
void try_unblock_process();

clock_t get_next_spawn_time();
unsigned int get_available_pid();
void print_blocked_processes();
void print_statistics();

#endif
