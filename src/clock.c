#include "clock.h"

#define ONE_BILLION 1000000000

void clock_increment(clock_t *clock, unsigned int ns) {
	clock->ns += ns;
	while (clock->ns >= ONE_BILLION) {
		clock->s += 1;
		clock->ns -= ONE_BILLION;
	}
}

int clock_compare(clock_t c1, clock_t c2) {
	if (c1.s > c2.s) return 1;
	if (c1.s == c2.s && c1.ns > c2.ns) return 1;
	if (c1.s == c2.s && c1.ns == c2.ns) return 0;
	return -1;
}
