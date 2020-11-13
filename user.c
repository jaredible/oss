#include <stdlib.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include "ipc.h"
#include "shared.h"
#include "user.h"

int main(int argc, char **argv) {
	init(argc, argv);
	
//	Time *system = (Time*) shm_lookup(0);
	
	return EXIT_SUCCESS;
}
