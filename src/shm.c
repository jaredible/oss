#include <sys/shm.h>
#include <sys/stat.h>

#include "helpers.h"
#include "shm.h"

int shm_allocate(int id, unsigned int size) {
	int shmid = shmget(key_from_id(id), size, IPC_CREAT | S_IRUSR | S_IWUSR);
	if (shmid == -1) crash("shmget");
	return shmid;
}

void *shm_attach(int shmid, bool readonly) {
	void *p = (void*) shmat(shmid, 0, readonly ? SHM_RDONLY : 0);
	if (!p) crash("shmat");
	return p;
}

void shm_cleanup(int shmid, void *p) {
	shm_detach(p);
	shm_deallocate(shmid);
}

void shm_detach(void *p) {
	if (shmdt(p) == -1) crash("shmdt");
}

void shm_deallocate(int shmid) {
	if (shmctl(shmid, IPC_RMID, 0) == -1) crash("shmctl");
}
