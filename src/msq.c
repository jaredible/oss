#include <sys/msg.h>
#include <sys/stat.h>

#include "helpers.h"
#include "msq.h"

int msq_allocate(int id) {
	int msqid = msgget(key_from_id(id), IPC_CREAT | S_IRUSR | S_IWUSR);
	if (msqid == -1) crash("shmget");
	return msqid;
}

void msq_send(int msqid, msg_t *msg, bool nowait) {
	if (msgsnd(msqid, msg, sizeof(msg_t), nowait ? IPC_NOWAIT : 0) == -1) crash("msgsnd");
}

void msq_receive(int msqid, msg_t *msg, bool nowait) {
	if (msgrcv(msqid, msg, sizeof(msg_t), msg->type, nowait ? IPC_NOWAIT : 0) == -1) crash("msgrcv");
}

void msq_deallocate(int msqid) {
	if (msgctl(msqid, IPC_RMID, 0) == -1) crash("msgctl");
}
