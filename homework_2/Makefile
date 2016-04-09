##****************************************************************************
## CMPE 207 (Network Programming and Applications) Sample Program.
##
## San Jose State University, Copyright (2016) Reserved.
##
## DO NOT REDISTRIBUTE WITHOUT THE PERMISSION OF THE INSTRUCTOR.
##****************************************************************************

CC=gcc
CCFLAGS=-g -std=c99 -D_POSIX_SOURCE -Wall

TARGETS=207httpd

all: $(TARGETS)

207httpd: 207httpd.o common.o
	$(CC) $(CCFLAGS) -pthread -o $@ $^

207httpd.o: 207httpd.c
	$(CC) $(CCFLAGS) -c $<

common.o: common.c
	$(CC) $(CCFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)

