CC := gcc
CFLAGS := -Wall -Werror -I.
LDFLAGS := -lrt -lpthread

all: binaries clean

binaries: manager simulator firealarm

manager: manager.o $(LDFLAGS)
manager.o: manager.c manager.h structs.h

simulator: simulator.o $(LDFLAGS)
simulator.o: simulator.c simulator.h structs.h

firealarm: firealarm.o $(LDFLAGS)
firealarm.o: firealarm.c

clean:
	rm -rf *.o

.PHONY: all clean
