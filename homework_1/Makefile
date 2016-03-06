##****************************************************************************
## CMPE 207 (Network Programming and Applications) Sample Program.
##
## San Jose State University, Copyright (2016) Reserved.
##
## DO NOT REDISTRIBUTE WITHOUT THE PERMISSION OF THE INSTRUCTOR.
##****************************************************************************

CC=gcc
CCFLAGS=-g -std=c99 -D_BSD_SOURCE -Wall

TARGETS=netcatd netcat spdtestd spdtest

all: $(TARGETS)

netcatd: netcatd.o common.o
	$(CC) $(CCFLAGS) -o $@ $^

netcatd.o: netcatd.c
	$(CC) $(CCFLAGS) -c $<

netcat: netcat.o common.o
	$(CC) $(CCFLAGS) -o $@ $^

netcat.o: netcat.c
	$(CC) $(CCFLAGS) -c $<

spdtestd: spdtestd.o common.o
	$(CC) $(CCFLAGS) -o $@ $^

spdtestd.o: spdtestd.c
	$(CC) $(CCFLAGS) -c $<

spdtest: spdtest.o common.o
	$(CC) $(CCFLAGS) -o $@ $^

spdtest.o: spdtest.c
	$(CC) $(CCFLAGS) -c $<

common.o: common.c
	$(CC) $(CCFLAGS) -c $<

clean:
	rm -rf *.o $(TARGETS)

