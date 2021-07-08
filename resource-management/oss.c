/*
 * oss.c November 21, 2020
 * Jared Diehl (jmddnb@umsystem.edu)
 */

#include <errno.h>
#include <libgen.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"
#include "queue.h"

#define log _log

/* Simulation functions */
void initSystem();
void initDescriptor();
void simulate();
void handleProcesses();
void trySpawnProcess();
void spawnProcess(int);
void initPCB(pid_t, int);
int findAvailablePID();
void advanceClock();
bool safe(Queue*, int, int[RESOURCES_MAX]);

/* Program lifecycle functions */
void init(int, char**);
void usage(int);
void registerSignalHandlers();
void signalHandler(int);
void timer(int);
void initIPC();
void freeIPC();

/* Utility functions */
void error(char*, ...);
void crash(char*);
void log(char*, ...);
void semLock(const int);
void semUnlock(const int);
void printDescriptor();
void printVector(char*, int[RESOURCES_MAX]);
void printMatrix(char*, Queue*, int[][RESOURCES_MAX], int);
void printSummary();

static char *programName;
static volatile bool quit = false;

/* IPC variables */
static int shmid = -1;
static int msqid = -1;
static int semid = -1;
static System *system = NULL;
static Message message;

/* Simulation variables */
static bool verbose = false;
static Queue *queue;
static Time nextSpawn;
static ResourceDescriptor descriptor;
static int activeCount = 0;
static int spawnCount = 0;
static int exitCount = 0;
static pid_t pids[PROCESSES_MAX];

int main(int argc, char **argv) {
	init(argc, argv);

	srand(time(NULL) ^ getpid());

	bool ok = true;

	/* Get program arguments */
	while (true) {
		int c = getopt(argc, argv, "hv");
		if (c == -1) break;
		switch (c) {
			case 'h':
				usage(EXIT_SUCCESS);
			case 'v':
				verbose = true;
				break;
			default:
				ok = false;
		}
	}

	/* Check for unknown arguments */
	if (optind < argc) {
		char buf[BUFFER_LENGTH];
		snprintf(buf, BUFFER_LENGTH, "found non-option(s): ");
		while (optind < argc) {
			strncat(buf, argv[optind++], BUFFER_LENGTH);
			if (optind < argc) strncat(buf, ", ", BUFFER_LENGTH);
		}
		error(buf);
		ok = false;
	}
	
	if (!ok) usage(EXIT_FAILURE);

	registerSignalHandlers();

	/* Clear log file */
	FILE *fp;
	if ((fp = fopen(PATH_LOG, "w")) == NULL) crash("fopen");
	if (fclose(fp) == EOF) crash("fclose");

	/* Setup simulation */
	initIPC();
	memset(pids, 0, sizeof(pids));
	system->clock.s = 0;
	system->clock.ns = 0;
	nextSpawn.s = 0;
	nextSpawn.ns = 0;
	initSystem();
	queue = queue_create();
	initDescriptor();

	/* Start simulating */
	simulate();

	printSummary();

	/* Cleanup resources */
	freeIPC();

	return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

void initSystem() {
	int i;

	/* Set default values in system data structures */
	for (i = 0; i < PROCESSES_MAX; i++) {
		system->ptable[i].spid = -1;
		system->ptable[i].pid = -1;
	}
}

void initDescriptor() {
	int i, j;

	/* Assign initial resources */
	for (i = 0; i < RESOURCES_MAX; i++)
		descriptor.resource[i] = (rand() % 10) + 1;

	/* Set shared resources */
	int shared = (SHARED_RESOURCES_MAX == 0) ? 0 : rand() % (SHARED_RESOURCES_MAX - (SHARED_RESOURCES_MAX - SHARED_RESOURCES_MIN)) + SHARED_RESOURCES_MIN;
	for (i = 0; i < shared; i++) {
		while (true) {
			j = rand() % RESOURCES_MAX;
			if (!descriptor.shared[j]) {
				descriptor.shared[j] = true;
				break;
			}
		}
	}
}

/* Simulation driver */
void simulate() {
	/* Simulate run loop */
	while (true) {
		trySpawnProcess();
		advanceClock();
		handleProcesses();
		advanceClock();

		/* Catch an exited user process */
		int status;
		pid_t pid = waitpid(-1, &status, WNOHANG);
		if (pid > 0) {
			int spid = WEXITSTATUS(status);
			pids[spid] = 0;
			activeCount--;
			exitCount++;
		}

		/* Stop simulating if the last user process has exited */
		if (quit) {
			if (exitCount == spawnCount) break;
		} else {
			if (exitCount == PROCESSES_TOTAL) break;
		}
	}
}

/* Sends and receives messages from user processes, and acts upon them */
void handleProcesses() {
	int i, n, count = 0;
	QueueNode *next = queue->front;
	
	/* While we have user processes to simulate */
	while (next != NULL) {
		advanceClock();

		/* Send a message to a user process saying it's your turn to "run" */
		int spid = next->index;
		message.type = system->ptable[spid].pid;
		message.spid = spid;
		message.pid = system->ptable[spid].pid;
		msgsnd(msqid, &message, sizeof(Message), 0);

		/* Receive a response of what they're doing */
		msgrcv(msqid, &message, sizeof(Message), 1, 0);

		switch (message.action) {
			case TERMINATE:
				log("%s: [%d.%d] Process P%d terminated\n", basename(programName), system->clock.s, system->clock.ns, message.spid);
				
				/* Release user process' resources */
				log("\tResources released: ");
				n = 0;
				for (i = 0; i < RESOURCES_MAX; i++)
					if (system->ptable[spid].allocation[i] > 0) n++;
				if (n > 0) {
					for (i = 0; i < RESOURCES_MAX; i++) {
						if (system->ptable[spid].allocation[i] > 0) {
							log("R%d:%d", i, system->ptable[spid].allocation[i]);
							if (i < RESOURCES_MAX - 1) log(", ");
						}
						system->ptable[spid].maximum[i] = 0;
						system->ptable[spid].allocation[i] = 0;
					}
					log("\n");
				} else {
					log("none\n");
				}
				
				/* Remove user process from queue */
				queue_remove(queue, spid);
				next = queue->front;
				for (i = 0; i < count; i++)
					next = (next->next != NULL) ? next->next : NULL;
				
				break;
			case REQUEST:
				log("%s: [%d.%d] Process P%d requesting resources\n", basename(programName), system->clock.s, system->clock.ns, message.spid);
				
				log("\tResources requested: ");
				for (i = 0; i < RESOURCES_MAX; i++) {
					if (message.request[i] > 0) {
						log("R%d:%d", i, message.request[i]);
						if (i < RESOURCES_MAX - 1) log(", ");
					}
				}
				log("\n");
				
				bool isSafe = safe(queue, spid, message.request);
				if (isSafe) {
					n = 0;
					for (i = 0; i < RESOURCES_MAX; i++)
						if (message.request[i] > 0) n++;
					
					for (i = 0; i < RESOURCES_MAX; i++) {
						system->ptable[spid].allocation[i] += message.request[i];
						message.request[i] = 0;
					}
					message.acquired = n > 0;
					log("Process P%d granted resources\n", message.spid);
				} else {
					message.acquired = false;
					log("Process P%d denied resources\n", message.spid);
				}
				
				message.type = system->ptable[spid].pid;
				msgsnd(msqid, &message, sizeof(Message), 0);
				
				break;
			case RELEASE:
				log("%s: [%d.%d] Process P%d releasing resources\n", basename(programName), system->clock.s, system->clock.ns, message.spid);
				
				log("\tResources released: ");
				n = 0;
				for (i = 0; i < RESOURCES_MAX; i++)
					if (system->ptable[spid].allocation[i] > 0) n++;
				if (n > 0) {
					for (i = 0; i < RESOURCES_MAX; i++) {
						if (system->ptable[spid].allocation[i] > 0) {
							log("R%d:%d", i, system->ptable[spid].allocation[i]);
							if (i < RESOURCES_MAX - 1) log(", ");
						}
						system->ptable[spid].allocation[i] = 0;
					}
					log("\n");
				} else {
					log("none\n");
				}
				
				break;
		}
		
		printDescriptor();
		
		/* Move on to the next user process */
		if (message.action == TERMINATE) continue;
		
		/* On to the next user process to simulate */
		count++;
		next = (next->next != NULL) ? next->next : NULL;
	}
}

/* Attempts to spawn a new user process, but depends on the simulation's current state */
void trySpawnProcess() {
	/* Guard statements checking if we can even attempt to spawn a user process */
	if (activeCount >= PROCESSES_MAX) return;
	if (spawnCount >= PROCESSES_TOTAL) return;
	if (nextSpawn.ns < 100) return;//(rand() % (500 + 1)) * 1000000) return;
	if (quit) return;
	
	/* Reset next spawn time */
	nextSpawn.ns = 0;

	/* Try to find an available simulated PID */
	int spid = findAvailablePID();

	/* Guard statement checking if we did find a PID */
	if (spid == -1) return;

	/* Now we can spawn a simulated user process */
	spawnProcess(spid);
}

void spawnProcess(int spid) {
	/* Fork a new user process */
	pid_t pid = fork();

	/* Record its PID */
	pids[spid] = pid;

	if (pid == -1) crash("fork");
	else if (pid == 0) {
		/* Since child, execute a new user process */
		char arg[BUFFER_LENGTH];
		snprintf(arg, BUFFER_LENGTH, "%d", spid);
		execl("./user", "user", arg, (char*) NULL);
		crash("execl");
	}

	/* Since parent, initialize the new user process for simulation */
	initPCB(pid, spid);
	queue_push(queue, spid);
	activeCount++;
	spawnCount++;

	log("%s: [%d.%d] Process P%d created\n", basename(programName), system->clock.s, system->clock.ns, spid);
}

void initPCB(pid_t pid, int spid) {
	int i;
	
	/* Set default values in a user process' data structure */
	PCB *pcb = &system->ptable[spid];
	pcb->pid = pid;
	pcb->spid = spid;
	for (i = 0; i < RESOURCES_MAX; i++) {
		pcb->maximum[i] = rand() % (descriptor.resource[i] + 1);
		pcb->allocation[i] = 0;
	}
}

/* Returns values [0-PROCESSES_MAX] for a found available PID, otherwise -1 for not found */
int findAvailablePID() {
	int i;
	for (i = 0; i < PROCESSES_MAX; i++)
		if (pids[i] == 0) return i;
	return -1;
}

void advanceClock() {
	semLock(0);

	/* Increment system clock by random nanoseconds */
	int r = rand() % (10 * 1000000) + 1;
	nextSpawn.ns += r;
	system->clock.ns += r;
	while (system->clock.ns >= (1000000000)) {
		system->clock.s++;
		system->clock.ns -= (1000000000);
	}

	semUnlock(0);
}

bool safe(Queue *queue, int index, int request[RESOURCES_MAX]) {
	int i, j, k, p;

	QueueNode *next = queue->front;
	if (next == NULL) return true;

	int count = queue_size(queue);
	int max[count][RESOURCES_MAX];
	int alloc[count][RESOURCES_MAX];
	int req[RESOURCES_MAX];
	int need[count][RESOURCES_MAX];
	int avail[RESOURCES_MAX];

	/* Copy user process' resource data */
	for (i = 0; i < count; i++) {
		p = next->index;
		for (j = 0; j < RESOURCES_MAX; j++) {
			max[i][j] = system->ptable[p].maximum[j];
			alloc[i][j] = system->ptable[p].allocation[j];
		}
		next = (next->next != NULL) ? next->next : NULL;
	}
	
	/* Calculate needed resources */
	for (i = 0; i < count; i++)
		for (j = 0; j < RESOURCES_MAX; j++)
			need[i][j] = max[i][j] - alloc[i][j];
	
	for (i = 0; i < RESOURCES_MAX; i++) {
		avail[i] = descriptor.resource[i];
		req[i] = request[i];
	}
	
	for (i = 0; i < count; i++)
		for (j = 0; j < RESOURCES_MAX; j++)
			if (!descriptor.shared[j]) avail[j] = avail[j] - alloc[i][j];
	
	if (verbose) {
		printVector("Request", request);
		printMatrix("Allocation", queue, alloc, count);
		printMatrix("Maximum", queue, max, count);
		printMatrix("Need", queue, need, count);
	}

	bool finish[count];
	int sequence[count];
	
	for (i = 0; i < count; i++)
		finish[i] = false;

	int work[RESOURCES_MAX];
	for (i = 0; i < RESOURCES_MAX; i++)
		work[i] = avail[i];
	
	i = 0;
	next = queue->front;
	while (next != NULL) {
		if (next->index == index) break;
		i++;
		next = (next->next != NULL) ? next->next : NULL;
	}
	
	/* Perform resource request algorithm */
	for (j = 0; j < RESOURCES_MAX; j++) {
		if (need[i][j] < req[j]) {
			log("%s: [%d.%d] Asked for more than initial max request\n", basename(programName), system->clock.s, system->clock.ns);

			if (verbose) {
				printVector("Available", avail);
				printMatrix("Need", queue, need, count);
			}

			return false;
		}

		if (req[j] <= avail[j]) {
			avail[j] -= req[j];
			alloc[i][j] += req[j];
			need[i][j] -= req[j];
		} else {
			log("%s: [%d.%d] Not enough available resources\n", basename(programName), system->clock.s, system->clock.ns);
			
			if (verbose) {
				printVector("Available", avail);
				printMatrix("Need", queue, need, count);
			}

			return false;
		}
	}
	
	/* Execute Banker's Algorithm */
	i = 0;
	while (i < count) {
		bool found = false;
		for (p = 0; p < count; p++) {
			if (!finish[p]) {
				for (j = 0; j < RESOURCES_MAX; j++)
					if (need[p][j] > work[j]) break;

				if (j == RESOURCES_MAX) {
					for (k = 0; k < RESOURCES_MAX; k++)
						work[k] += alloc[p][k];

					sequence[i++] = p;
					finish[p] = true;
					found = true;
				}
			}
		}

		if (!found) {
			log("%s [%d.%d] System is in UNSAFE state!\n", basename(programName), system->clock.s, system->clock.ns);
			return false;
		}
	}

	if (verbose) {
		printVector("Available", avail);
		printMatrix("Need", queue, need, count);
	}

	i = 0;
	int temp[count];
	next = queue->front;
	while (next != NULL) {
		temp[i++] = next->index;
		next = (next->next != NULL) ? next->next : NULL;
	}

	log("%s [%d.%d] System is in SAFE state. Safe sequence: ", basename(programName), system->clock.s, system->clock.ns);
	for (i = 0; i < count; i++)
		log("%2d ", temp[sequence[i]]);
	log("\n");

	return true;
}

void init(int argc, char **argv) {
	programName = argv[0];

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);
}

void usage(int status) {
	if (status != EXIT_SUCCESS) fprintf(stderr, "Try '%s -h' for more information\n", programName);
	else {
		printf("Usage: %s [-v]\n", programName);
		printf("   v : Verbose mode (default off)\n");
	}
	exit(status);
}

void registerSignalHandlers() {
	struct sigaction sa;

	/* Set up SIGINT handler */
	if (sigemptyset(&sa.sa_mask) == -1) crash("sigemptyset");
	sa.sa_handler = &signalHandler;
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGINT, &sa, NULL) == -1) crash("sigaction");

	/* Set up SIGALRM handler */
	if (sigemptyset(&sa.sa_mask) == -1) crash("sigemptyset");
	sa.sa_handler = &signalHandler;
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGALRM, &sa, NULL) == -1) crash("sigaction");

	/* Initialize timout timer */
	timer(TIMEOUT);

	signal(SIGUSR1, SIG_IGN);
	signal(SIGSEGV, signalHandler);
	signal(SIGABRT, signalHandler);
}

void signalHandler(int sig) {
	if (sig == SIGALRM) quit = true;
	else {
		printf("%d\n", sig);
		printSummary();
		
		/* Kill all running user processes */
		kill(0, SIGUSR1);
		while (wait(NULL) > 0);
		
		freeIPC();
		exit(EXIT_SUCCESS);
	}
}

void timer(int duration) {
	struct itimerval val;
	val.it_value.tv_sec = duration;
	val.it_value.tv_usec = 0;
	val.it_interval.tv_sec = 0;
	val.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &val, NULL) == -1) crash("setitimer");
}

void initIPC() {
	key_t key;

	if ((key = ftok(KEY_PATHNAME, KEY_ID_SYSTEM)) == -1) crash("ftok");
	if ((shmid = shmget(key, sizeof(System), IPC_EXCL | IPC_CREAT | PERMS)) == -1) crash("shmget");
	if ((system = (System*) shmat(shmid, NULL, 0)) == (void*) -1) crash("shmat");

	if ((key = ftok(KEY_PATHNAME, KEY_ID_MESSAGE_QUEUE)) == -1) crash("ftok");
	if ((msqid = msgget(key, IPC_EXCL | IPC_CREAT | PERMS)) == -1) crash("msgget");

	if ((key = ftok(KEY_PATHNAME, KEY_ID_SEMAPHORE)) == -1) crash("ftok");
	if ((semid = semget(key, 1, IPC_EXCL | IPC_CREAT | PERMS)) == -1) crash("semget");
	if (semctl(semid, 0, SETVAL, 1) == -1) crash("semctl");
}

void freeIPC() {
	if (system != NULL && shmdt(system) == -1) crash("shmdt");
	if (shmid > 0 && shmctl(shmid, IPC_RMID, NULL) == -1) crash("shmdt");

	if (msqid > 0 && msgctl(msqid, IPC_RMID, NULL) == -1) crash("msgctl");

	if (semid > 0 && semctl(semid, 0, IPC_RMID) == -1) crash("semctl");
}

void error(char *fmt, ...) {
	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);
	
	fprintf(stderr, "%s: %s\n", programName, buf);
	
	freeIPC();
}

void crash(char *msg) {
	char buf[BUFFER_LENGTH];
	snprintf(buf, BUFFER_LENGTH, "%s: %s", programName, msg);
	perror(buf);
	
	freeIPC();
	
	exit(EXIT_FAILURE);
}

void log(char *fmt, ...) {
	FILE *fp = fopen(PATH_LOG, "a+");
	if(fp == NULL) crash("fopen");

	char buf[BUFFER_LENGTH];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, BUFFER_LENGTH, fmt, args);
	va_end(args);

	fprintf(stdout, buf);
	fprintf(fp, buf);

	if (fclose(fp) == EOF) crash("fclose");
}

void semLock(const int index) {
	struct sembuf sop = { index, -1, 0 };
	if (semop(semid, &sop, 1) == -1) crash("semop");
}

void semUnlock(const int index) {
	struct sembuf sop = { index, 1, 0 };
	if (semop(semid, &sop, 1) == -1) crash("semop");
}

void printDescriptor() {
	int i, j, p;
	
	QueueNode *next = queue->front;
	if (next == NULL) return;
	
	int count = queue_size(queue);
	int max[count][RESOURCES_MAX];
	int alloc[count][RESOURCES_MAX];
	int avail[RESOURCES_MAX];
	
	for (i = 0; i < count; i++) {
		p = next->index;
		for (j = 0; j < RESOURCES_MAX; j++) {
			max[i][j] = system->ptable[p].maximum[j];
			alloc[i][j] = system->ptable[p].allocation[j];
		}
		next = (next->next != NULL) ? next->next : NULL;
	}
	
	for (i = 0; i < RESOURCES_MAX; i++)
		avail[i] = descriptor.resource[i];
	for (i = 0; i < count; i++)
		for (j = 0; j < RESOURCES_MAX; j++)
			avail[j] -= alloc[i][j];
	
	printVector("Total", descriptor.resource);
	printVector("Shared", descriptor.shared);
	printVector("Available", avail);
	printMatrix("Allocation", queue, alloc, count);
	printMatrix("Maximum", queue, max, count);
}

void printVector(char *title, int vector[RESOURCES_MAX]) {
	int i;
	
	log("%s\n     ", title);
	for (i = 0; i < RESOURCES_MAX; i++)
		log(" R%-2d", i);
	log("\n    ");
	for (i = 0; i < RESOURCES_MAX; i++) {
		log(" %2d", vector[i]);
		if (i < RESOURCES_MAX - 1) log(" ");
	}
	log("\n");
}

void printMatrix(char *title, Queue *queue, int matrix[][RESOURCES_MAX], int count) {
	QueueNode *next;
	next = queue->front;

	int i, j;
	log("%s\n     ", title);
	for (i = 0; i < RESOURCES_MAX; i++)
		log(" R%-2d", i);
	log("\n");
	for (i = 0; i < count; i++) {
		log("P%-2d ", next->index);
		for (j = 0; j < RESOURCES_MAX; j++) {
			log(" %2d", matrix[i][j]);
			if (j < RESOURCES_MAX - 1) log(" ");
		}
		log("\n");

		next = (next->next != NULL) ? next->next : NULL;
	}
}

void printSummary() {
	log("\n\nSystem time: %d.%d\n", system->clock.s, system->clock.ns);
	log("Total processes executed: %d\n", spawnCount);
}
