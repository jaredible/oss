#include <stdio.h>

#include "helper.h"
#include "shared.h"

int getLineCount(char *path) {
	FILE *fp = fopen(path, "r");
	if (fp == NULL) crash("fopen");
	
	int count = 0;
	char c;
	for (c = getc(fp); c != EOF; c = getc(fp))
		if (c == '\n') count++;
	
	fclose(fp);
	
	return count;
}
