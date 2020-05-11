CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lrt -lpthread
BIN=proj2
SOURCES=proj2.c
RM=rm

.PHONY: clean

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(BIN) $(LFLAGS)

clean:
	$(RM) -f *.o core *.out

cleanall: clean
	$(RM) $(BIN)

zip: proj2.c Makefile
	zip proj2.zip *.c Makefile

