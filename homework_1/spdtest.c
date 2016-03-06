/* Author: Sanisha Rehan
 *
 * spdtest.c: Example client program to implement an Iterative UDP client used for
 * determining network latency.
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

int	UDPLatency(const char *host, const char *service, int msg_len);
int	errexit(const char *format, ...);
int	connectUDP(const char *host, const char *service);

#define	MICROSECS		1000000
#define MSGCOUNT		30

/*------------------------------------------------------------------------
 * main - UDP client for network speed test service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	char *host = "localhost";		/* Default value for host */
	char *service = "spdtest";		/* Default name for service */
	int msg_len;

	switch (argc) {
	case 4:
		host = argv[1];
		service = argv[2];
		msg_len = atoi(argv[3]);
		break;
	default:
		fprintf(stderr, "You need to pass host machine, port number and message \
			length \n");
		fprintf(stderr, "usage: spdtst host_address port_number msg_length\n");
		exit(1);
	}
	UDPLatency(host, service, msg_len);
	exit(0);
}

/*
 * This function should pass messages to client, collect ack
 * and calculate the latency as per given formula.
*/
int
UDPLatency(const char *host, const char *service, int msg_len) {
	char *buf;
	
	int sock;	
	char msg_count = 0;
	struct timeval client_start_time, client_end_time;
	long int cst_usecs, cet_usecs;
	
	/* Create msg buffer of specified length here */
	buf = (char *) malloc(sizeof(char) * msg_len);
	memset(buf, 'M', msg_len); 

	/* Allocate a socket */
	sock = connectUDP(host, service);

	/* 1. Note time before sending the messages */
	gettimeofday(&client_start_time, NULL);
	cst_usecs = (client_start_time.tv_sec * MICROSECS + client_start_time.tv_usec);

	while(msg_count < MSGCOUNT) {
		msg_count++;
		
		buf[0] = msg_count;
		write(sock, buf, msg_len);

		/* 2. Read the ack back from server */
		if(read(sock, buf, sizeof(buf)) < 0) {
			errexit("Socket read failed %s \n", strerror(errno));
		}
	}

	/* 3. Note time after all messages are received */
	gettimeofday(&client_end_time, NULL);
	cet_usecs = (client_end_time.tv_sec * MICROSECS + client_end_time.tv_usec);

	/* 4. Calculate throughput number */
	long int total_time_msecs = (cet_usecs - cst_usecs)/1000;
	long int throughput = (MSGCOUNT * msg_len * 1000)/total_time_msecs;	
	
	fprintf(stdout, "Total time taken for %d messages: %ld ms; Throughtput of client with msg size: %d is %ld\n", MSGCOUNT, total_time_msecs, msg_len, throughput);	
	
	/* 4. Close socket and free memory */
	free(buf);
	close(sock);

	return 0;
}
