##****************************************************************************
## Mind Sync Game Makefile
##
##****************************************************************************

CC=g++
CCFLAGS=-g -std=c++11 -D_DEFAULT_SOURCE -Wall

TARGETS=mindSync

all: $(TARGETS)

mindSync: mind_sync.o mind_sync_server.o server_common.o
	$(CC) $(CCFLAGS) -pthread -o $@ $^ -lsqlite3

mind_sync.o: mind_sync.cpp server_common.o
	$(CC) $(CCFLAGS) -c $<

mind_sync_server.o: mind_sync_server.cpp
	$(CC) $(CCFLAGS) -c $<

server_common.o: server_common.cpp
	$(CC) $(CCFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)

