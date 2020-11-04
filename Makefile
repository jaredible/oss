CC		= gcc
CFLAGS		= -Wall -g -lm

OSS_SRC		= oss.c
OSS_OBJ		= $(OSS_SRC:.c=.o) $(HELPER_OBJ) $(QUEUE_OBJ)
OSS		= oss

USER_SRC	= user.c
USER_OBJ	= $(USER_SRC:.c=.o) $(HELPER_OBJ)
USER		= user

SHARED_HEAD	= shared.h
SHARED_OBJ	= shared.o

HELPER_HEAD	= helper.h
HELPER_OBJ	= helper.o

QUEUE_HEAD	= queue.h
QUEUE_OBJ	= queue.o

OUTPUT		= $(OSS) $(USER)

all: $(OUTPUT)

$(OSS): $(OSS_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $(OSS_OBJ) $(SHARED_OBJ) -o $(OSS)

$(USER): $(USER_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $(USER_OBJ) $(SHARED_OBJ) -o $(USER)

%.o: %.c $(SHARED_HEAD)
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.PHONY: clean
clean:
	/bin/rm -f $(OUTPUT) *.o *.log
