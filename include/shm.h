#ifndef _SHM_H
#define _SHM_H

#include <stdbool.h>

int shm_allocate(int, unsigned int);
void *shm_attach(int, bool);
void shm_cleanup(int, void*);
void shm_detach(void*);
void shm_deallocate(int);

#endif
