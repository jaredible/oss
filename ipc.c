#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "error.h"
#include "ipc.h"

#define PERMS (S_IRUSR | S_IWUSR)

key_t getKeyFromId(int id) {
	key_t key = ftok(".", id);
	if (key == -1) crash("ftok");
	return key;
}

void *shm_create(size_t bytes, int *id) {
	int shmid = shmget(getKeyFromId(*id++), bytes, IPC_CREAT | IPC_EXCL | PERMS);
	if (shmid == -1) crash("shmget");
	void *addr = shmat(shmid, NULL, 0);
	if (addr == (void*) -1) crash("shmat");
	return addr;
}

int shm_lookup(int id) {
	int shmid = shmget(getKeyFromId(id), 0, 0);
	if (shmid == -1) crash("shmget");
	return shmid;
}

void *shm_attach(int id) {
	void *addr = shmat(shm_lookup(id), NULL, 0);
	if (addr == (void*) -1) crash("shmat");
	return addr;
}

void shm_detach(const void *shmptr) {
	if (shmdt(shmptr) == -1) crash("shmdt");
}

void shm_remove(int id) {
	if (shmctl(shm_lookup(id), IPC_RMID, NULL) == -1) crash("shmctl");
}
