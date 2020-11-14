#ifndef TIME_H
#define TIME_H

typedef struct {
	unsigned int s;
	unsigned int ns;
} Time;

void addTime(Time*, int);
Time subTime(Time, Time);
int compareTime(Time, Time);

#endif
