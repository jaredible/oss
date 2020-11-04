#ifndef OSS_H
#define OSS_H

#include <stdio.h>

#include "shared.h"

void showPCB(PCB *pcb) {
	printf("LocalPID: %d\n", pcb->localPID);
	printf("ActualPID: %d\n", pcb->actualPID);
	printf("Priority: %d\n", pcb->priority);
	printf("Arrival time: %d:%d\n", pcb->arrival.sec, pcb->arrival.ns);
	printf("Exit time: %d:%d\n", pcb->exit.sec, pcb->exit.ns);
	printf("CPU time: %d:%d\n", pcb->cpu.sec, pcb->cpu.ns);
	printf("Wait time: %d:%d\n", pcb->wait.sec, pcb->wait.ns);
	printf("System time: %d:%d\n", pcb->system.sec, pcb->system.ns);
}

#endif
