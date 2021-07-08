CC		= gcc
CFLAGS		= -Wall -g -lm

OSS_SRC		= oss.c
OSS_OBJ		= $(OSS_SRC:.c=.o) $(SHARED_OBJ) $(QUEUE_OBJ)
OSS		= oss

USER_SRC	= user.c
USER_OBJ	= $(USER_SRC:.c=.o) $(SHARED_OBJ)
USER		= user

SHARED_OBJ	= shared.o

QUEUE_OBJ	= queue.o

OUTPUT		= $(OSS) $(USER)

all: $(OUTPUT)

$(OSS): $(OSS_OBJ)
	$(CC) $(CFLAGS) $(OSS_OBJ) -o $(OSS)

$(USER): $(USER_OBJ)
	$(CC) $(CFLAGS) $(USER_OBJ) -o $(USER)

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.PHONY: clean
clean:
	/bin/rm -f $(OUTPUT) *.o *.log
