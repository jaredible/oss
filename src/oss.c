#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "constants.h"
#include "helpers.h"
#include "msq.h"
#include "oss.h"
#include "queue.h"
#include "shm.h"

int system_clock_id;
clock_t *system_clock;
pid_t *child_pids;
queue_t *blocked;

int spawn_count = 0;
int active_count = 0;
int exit_count = 0;

int main(int argc, char *argv[]) {
	register_signal_handlers();
	
	system_clock_id = shm_allocate(SYSTEM_CLOCK_KEY_ID, sizeof(clock_t));
	
	set_timer(TIMEOUT);
	srand(time(NULL) ^ getpid());
	
	blocked = (queue_t*) malloc(sizeof(queue_t) * MAX_RESOURCES);
	// TODO: init blocked
	
	oss();
	
	cleanup();
	
	return EXIT_SUCCESS;
}

void register_signal_handlers() {
	struct sigaction act;
	act.sa_handler = handle_interrupt; // Signal handler
	if (sigemptyset(&act.sa_mask) == -1) crash("sigemptyset"); // No other signals should be blocked
	act.sa_flags = 0; // 0 to not modify behavior
	if (sigaction(SIGINT, &act, NULL) == -1) crash("sigaction");
	
	act.sa_handler = handle_alarm; // Signal handler
	if (sigemptyset(&act.sa_mask) == -1) crash("sigemptyset"); // No other signals should be blocked
	if (sigaction(SIGALRM, &act, NULL) == -1) crash("sigaction");
}

void handle_interrupt(int sig) {
	printf("SIGINT\n");
	cleanup();
}

void handle_alarm(int sig) {
	printf("SIGALRM\n");
	cleanup();
}

void terminate_children() {
	int i;
	for (i = 0; i < MAX_PROCESSES; i++) {
		if (child_pids[i] == -1) continue;
		if (kill(child_pids[i], SIGTERM) < 0 && errno != ESRCH) crash("kill");
	}
	free(child_pids);
}

void await_children() {
}

void cleanup() {
}

void oss() {
	clock_t next_spawn = get_next_spawn_time();
	
	while (true) {
		try_spawn(&next_spawn);
	}
}

void try_spawn(clock_t *next_spawn) {
	if (clock_compare(*system_clock, *next_spawn) < 0) return;
	if (active_count == MAX_PROCESSES) return;
	if (spawn_count == TOTAL_PROCESSES) return;
	
	*next_spawn = get_next_spawn_time();
	
	int spid = get_available_pid();
	if (spid == -1) return;
	
//	set_maximum_claims(spid);
	spawn(spid);
	active_count++;
	spawn_count++;
}

void spawn(unsigned int spid) {
	if ((child_pids[spid] = fork()) == 0) {
		char *args[EXECV_SIZE];
		args[0] = "./user";
		args[EXECV_SIZE - 1] = NULL;
		
		char arg1[10];
		char arg2[10];
		char arg3[10];
		char arg4[3];
		
		sprintf(arg1, "%d", system_clock_id);
		sprintf(arg2, "%d", 0);
		sprintf(arg3, "%d", 0);
		sprintf(arg4, "%d", spid);
		
		args[SYSTEM_CLOCK_ID_INDEX] = arg1;
		args[RESOURCE_DESCRIPTOR_ID_INDEX] = arg2;
		args[MESSAGE_QUEUE_ID_INDEX] = arg3;
		args[PID_ID_INDEX] = arg4;
		
		execvp(args[0], args);
	}
	
	if (child_pids[spid] == -1) crash("fork");
}

void check_messages() {
}

void handle_message() {
}

void try_unblock_process(int rid, msg_t msg) {
	if (queue_empty(&blocked[rid])) return;
	
	bool available = is_resource_available(rid);
	if (!available) return;
	
	int spid = queue_peek(&blocked[rid]);
	
	bool safe = bankers_algorithm(spid, rid);
	clock_increment(system_clock, get_work());
	
	if (safe) {
		// TODO: print
		// TODO: update allocated
		queue_pop(&blocked[rid]);
		// TODO: print allocated
		// TODO: send message
	}
}

clock_t get_next_spawn_time(clock_t system_clock) {
	clock_increment(&system_clock, rand() % (500 * 1000000));
	return system_clock;
}

unsigned int get_available_pid() {
	int i;
	for (i = 0; i < MAX_PROCESSES; i++) {
		if (child_pids[i] > 0) continue;
		return i;
	}
	return -1;
}
