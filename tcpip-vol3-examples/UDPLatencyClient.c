/* Author: Sanisha Rehan
 *
 * Example client program to implement a UDP client used for
 * determining network latency.
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

int	UDPLatency(const char *host, const char *service, int msg_len);
int	errexit(const char *format, ...);
int	connectUDP(const char *host, const char *service);

#define	MICROSECS		1000000

/*------------------------------------------------------------------------
 * main - UDP client for ECHO service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	char *host = "localhost";
	char *service = "latency";
	int msg_len;

	switch (argc) {
	case 4:
		host = argv[1];
		service = argv[2];
		msg_len = atoi(argv[3]);
		break;
	default:
		fprintf(stderr, "You need to pass host machine, port number and messge length \n");
		fprintf(stderr, "usage: UDPecho [host [port]]\n");
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
	char *buf ;
	int char_count = 0;
	buf = (char *) malloc((sizeof(char) * msg_len)+1); 
	int sock, nchars;	
	int msg_count = 0;
	struct timeval start_time, end_time, client_start_time, client_end_time;
	long int st_usecs, et_usecs, cst_usecs, cet_usecs;

	/* Generate msg of specified length here */
	for (char_count=0; char_count < msg_len; char_count++) {
		buf[char_count] = 'M';
	}
	
	buf[msg_len] = '\0';
	fprintf(stderr, "The size of buffer is %d and %d\n", sizeof(buf), strlen(buf));

	/* Allocate a socket */
	sock = connectUDP(host, service);

	/* Note time before sending the messages */
	gettimeofday(&client_start_time, NULL);
	cst_usecs = (client_start_time.tv_sec * MICROSECS + client_start_time.tv_usec);

	while(msg_count < 30) {
		msg_count++;
		nchars = strlen(buf);
		write(sock, buf, nchars);
	
		/* Read time when request sent to server */
		gettimeofday(&start_time, NULL);
		st_usecs = (start_time.tv_sec * MICROSECS + start_time.tv_usec);

		/* Read the ack back from server */
		if(read(sock, buf, nchars) < 0) {
			errexit("Socket read failed %s \n", strerror(errno));
		}
		
		gettimeofday(&end_time, NULL);
		et_usecs = (end_time.tv_sec * MICROSECS + end_time.tv_usec);

		float delay = et_usecs - st_usecs;
		
		/* Print the result on stdout */
		fprintf(stdout, "Start time: %ld, end time: %ld \n", st_usecs, et_usecs);
		fprintf(stdout, "The ack received is %s, count: %d after delay of: %4.4f ms\n",
			 buf, msg_count, (delay/1000.0));
	}

	/* Note time after all messages are received */
	gettimeofday(&client_end_time, NULL);
	cet_usecs = (client_end_time.tv_sec * MICROSECS + client_end_time.tv_usec);

	long int total_time_msecs = (cet_usecs - cst_usecs)/1000;
	long int through = (30 * msg_len * 1000);
	long int throughput = through/total_time_msecs;

	fprintf(stdout, "msg len: %d, microsecs: %d, total time spent: %ld ms\n", msg_len, MICROSECS, total_time_msecs);
	fprintf(stdout, "30 * msg_len * MICROSECS : %ld \n", (30 * msg_len * MICROSECS));
	fprintf(stdout, "throughput : %ld \n", throughput);
	fprintf(stdout, "655228928/41295: %ld \n", (655228928/41295));
	
	fprintf(stdout, "\nTotal time spent between start time %ld and end time %ld is: %ld, Throughtput of client with msg size : %d is %ld \n", cst_usecs, cet_usecs, total_time_msecs, msg_len, throughput);	
	free(buf);
	close(sock);

}
