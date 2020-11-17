#ifndef CLOCK_H
#define CLOCK_H

#define clock_t _clock_t

#define ONE_BILLION 1000000000

typedef struct {
	unsigned long s;
	unsigned long ns;
} clock_t;

void clock_increment(clock_t*, unsigned int);
int clock_compare(clock_t, clock_t);

#endif
