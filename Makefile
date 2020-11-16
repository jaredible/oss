CC		= gcc
CFLAGS		= -Wall -g

OSS		= oss
OSS_SRC		= oss.c
OSS_OBJ		= $(OSS_SRC:.c=.o) descriptor.o queue.o shared.o time.o

USER		= user
USER_SRC	= user.c
USER_OBJ	= $(USER_SRC:.c=.o) descriptor.o shared.o time.o

OUTPUT		= $(OSS) $(USER)

.PHONY: all clean

all: $(OUTPUT)

$(OSS): $(OSS_OBJ)
	$(CC) $(CFLAGS) $(OSS_OBJ) -o $(OSS)

$(USER): $(USER_OBJ)
	$(CC) $(CFLAGS) $(USER_OBJ) -o $(USER)

%.o: %.c
	$(CC) $(CFLAGS) -c $*.c -o $*.o

clean:
	/bin/rm -f $(OUTPUT) *.o *.log
