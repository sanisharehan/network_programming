/* Author: Sanisha Rehan
*
* netcat.c: Client to get data from stdin and send to server on a specified port
* similar to netcat service.
*
*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int TCPnetcat(const char *host, const char *service);
int	errexit(const char *format, ...);
int	connectTCP(const char *host, const char *service);

#define	BUFSIZE	1

/*------------------------------------------------------------------------
 * main - TCP client for netcat service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	char	*host = "localhost";	/* host to use if none supplied	*/
	char	*service;

	switch (argc) {
	case 3:
		host = argv[1];
		service = argv[2];
		break;
	default:
		fprintf(stderr, "usage: netcat host port\n");
		exit(1);
	}
	TCPnetcat(host, service);
	exit(0);
}

/*
 * Function to read data from stdin and send to server. Data is read byte by
 * byte and sent to TCP server.
*/
int
TCPnetcat(const char *host, const char *service) {
	char buf[BUFSIZE + 1];
	int sd, bytes_read;
	
	/* Connect to TCP socket */
	sd = connectTCP(host, service);

	/* Read data from stdin and write through the network */
	while((bytes_read = fread(buf, 1, BUFSIZE, stdin)) > 0) {
		write(sd, buf, bytes_read);   /* Write to server */
	}
	return 0;
}
