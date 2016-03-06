/* Author: Sanisha Rehan 
 *
 * spdtestd.c: This is the server example for iterative UDP. This server is 
 * used to test network speed for data transmission over UDP transport protocol.
 *
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

#define MSGCOUNT		30

int passiveUDP(const char *service);
int errexit(const char *format, ...);

/*-----------------------------------------------------------------------------
 * Main server program. This should perform following:
 * a. Take 3 arguments as input, namely:
 *	  i.   Service port number
 *   ii.   Message size in bytes
 *	iii.   Latency/wait time in ms
 * b. After reading the message from client, the server should sleep for 
 *		specified time and then send ack to the client.
 * c. Server should do this for 30 messages and then exit
-----------------------------------------------------------------------------*/
int
main(int argc, char *argv[]) {
	struct sockaddr_in client_sock;     /* Client address */
	unsigned int alen;					/* Length of client addr */

	unsigned int wait_time;				/* Server latency time in ms */
	int msg_size;						/* Message size in bytes */
	int sock;
	char *service = "spdtest";

	int msg_count = 0;
	int bytes_received;
	char *buf;

	/* Read command line arguments */
	switch (argc) {
	case 4:
		service = argv[1];
		msg_size = atoi(argv[2]);
		wait_time = atoi(argv[3]);
		break;
	default:
		fprintf(stderr, "Need to pass port number, message length and wait time \n");
		errexit("Usage error: spdtest port msg_size wait_time\n");
	}

	sock = passiveUDP(service);	
	
	/* Allocate buffer of specified msg size */
	buf = (char *) malloc(sizeof(char) * msg_size);

	/* Receive data from connection and send ack after delay */
	while(msg_count < MSGCOUNT) {
		msg_count++;
		alen = sizeof(client_sock);
		
		bytes_received = recvfrom(sock, buf, msg_size, 0,
			(struct sockaddr *)&client_sock, &alen);
		if (bytes_received < 0) {
			errexit("Error in recvfrom: %s \n", strerror(errno));
		}
		
		/* Introduce delay */
		usleep(wait_time * 1000);  			/* Since this function accepts useconds*/

		/* Send the ack back inluding unique number from received msg */
		char response[10] = "Ack:";
		
		/* Read the sequence number from client's msg */
		response[4] = buf[0];
		response[5] = '\0';
		sendto(sock, (char *) response, strlen(response), 0,
			(struct sockaddr *) &client_sock, sizeof(client_sock));
	}
	
	/* Free memory and close socket */
	free(buf);
	close(sock);
}
