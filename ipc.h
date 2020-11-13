#ifndef IPC_H
#define IPC_H

void *shm_create(size_t, int*);
int shm_allocate(int);
void *shm_attach(int);
void shm_detach(const void*);
void shm_remove(int);

void sem_create();
int sem_lookup();
void sem_remove();
void sem_lock(int, int);
void sem_unlock(int, int);

void msg_create();
void msg_lookup();
void msg_attach();

#endif
