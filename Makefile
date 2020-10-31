CC		= gcc
CFLAGS		= -Wall -g

OSS_SRC		= oss.c
OSS_OBJ		= $(OSS_SRC:.c=.o) $(QUEUE_OBJ)
OSS		= oss

USER_SRC	= user.c
USER_OBJ	= $(USER_SRC:.c=.o)
USER		= user

SHARED_SRC	= shared.h
SHARED_OBJ	= shared.o

HELPER_SRC	= helper.h
HELPER_OBJ	= helper.o

QUEUE_SRC	= queue.h
QUEUE_OBJ	= queue.o

OUTPUT		= $(OSS) $(USER)

all: $(OUTPUT)

$(OSS): $(OSS_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $(OSS_OBJ) $(SHARED_OBJ) -o $(OSS)

$(USER): $(USER_OBJ) $(SHARED_OBJ)
	$(CC) $(CFLAGS) $(USER_OBJ) $(SHARED_OBJ) -o $(USER)

%.o: %.c $(SHARED_SRC)
	$(CC) $(CFLAGS) -c $*.c -o $*.o

.PHONY: clean
clean:
	/bin/rm -f $(OUTPUT) *.o *.out
