#include "time.h"

void addTime(Time *time, int ns) {
	time->ns += ns;
	while (time->ns >= 1e9) {
		time->ns -= 1e9;
		time->s += 1;
	}
}

Time subTime(Time a, Time b) {
	Time diff = { .s = a.s - b.s, .ns = a.ns - b.ns };
	while (diff.ns < 0) {
		diff.ns += 1e9;
		diff.s -= 1;
	}
	return diff;
}

int compareTime(Time a, Time b) {
	unsigned long t1 = a.s * 1e9 + a.ns;
	unsigned long t2 = b.s * 1e9 + b.ns;
	if (t1 < t2) return -1;
	if (t1 > t2) return 1;
	return 0;
}
