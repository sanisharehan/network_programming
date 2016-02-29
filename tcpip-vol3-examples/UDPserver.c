/* Author: Sanisha Rehan 
 *
 * This is the server example for iterative UDP.
 *
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

int UDPLatencyGenerator(int sock, int msg_size, int wait_time);
int passiveUDP(const char *service);
int errexit(const char *format, ...);

/***********************************
 * main server program. This should perform following:
* a. Take 3 arguments as input, namely:
*	i.  Service port number
*	ii. Message size in bytes
*	iii.Latency/wait time in ms
* b. After reading the message from client, the server should sleep for specified time and then send ack to the client.
* c. Server should do this for 30 messages and then exit
*/
int
main(int argc, char *argv[]) {
	struct sockaddr_in client_sock;     /* Client address */
	char *service = "latency";
	unsigned int wait_time;		/* Latency time sent from client */
	int msg_size = 1;		/* Message size in bytes */

	int sock;
	unsigned int alen;	/* Length of client addr */
    
    char *endpt;

	switch (argc) {
	case 4:
		service = argv[1];
		msg_size = strtol(argv[2], &endpt, 10);
		wait_time = strtol(argv[3], &endpt, 10);
		break;
	default:
		fprintf(stderr, "Need to pass port number, message lenth and wait time \n");
		errexit("Usage error in UDPserver");
	}

	sock = passiveUDP(service);
	
	fprintf(stderr, "Passed argumets are %s, %d, %u \n", service, msg_size, wait_time);
	/* Call the function to perform task of server */
	/* UDPLatencyGenerator(sock, msg_size, wait_time); */
	
	int count = 0;
	int bytes_received;

	char *buf;
	buf = (char *) malloc((sizeof(char) * msg_size) + 1);

	while(count < 30) {
		count ++;
		alen = sizeof(client_sock);
		bytes_received = recvfrom(sock, buf, msg_size, 0,
			(struct sockaddr *)&client_sock, &alen);
		if (bytes_received < 0) {
			errexit("Error in recvfrom: %s \n", strerror(errno));
		} else {
			fprintf(stderr, "Number of bytes received: %d \n", bytes_received);
		}
		
		/* Introduce delay */
		usleep(wait_time * 1000);  /* Since this function accepts microseconds */

		/* Send the ack back */
		char response[10] = "Ack ";
		sendto(sock, (char *) response, sizeof(response), 0,
			(struct sockaddr *) &client_sock, sizeof(client_sock));
		fprintf(stderr, "Ack sent after some delay of %d ms \n", wait_time);	
	}
	
	free(buf);
	close(sock);
}
