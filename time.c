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
	return 0;
}
