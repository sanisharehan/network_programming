/* Author: Sanisha Rehan
*
* Client to get data from stdin and send to server
* on a specified port, similar to netcat service.
*
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int TCPnetcat(const char *host, const char *service);
int	errexit(const char *format, ...);
int	connectTCP(const char *host, const char *service);

#define	BUFSIZE	100

/*------------------------------------------------------------------------
 * main - TCP client for netcat service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	char	*host = "localhost";	/* host to use if none supplied	*/
	char	*service = "netcat";	/* default service name	*/

	switch (argc) {
	case 1:
		break;
	case 3:
		service = argv[2];
		/* FALL THROUGH */
	case 2:
		host = argv[1];
		break;
	default:
		fprintf(stderr, "usage: TCPnetcat [host [port]]\n");
		exit(1);
	}
	TCPnetcat(host, service);
	exit(0);
}

/*
 * Function to read data from stdin and send to server
*/
int
TCPnetcat(const char *host, const char *service) {
	char buf[BUFSIZE + 1];
	int sd, bytes_read;
	
	/* Connect to TCP socket */
	sd = connectTCP(host, service);

	/* Read data from stdin and send through the network */
	while((bytes_read = fread(buf, 1, BUFSIZE, stdin)) > 0) {
		/* buf[LINELEN] = '\0' ; */
	    /*fprintf(stderr, "Number of bytes read: %d \n", bytes_read);
		fprintf(stderr, "Data in buf: %s \n", buf) */
		write(sd, buf, bytes_read);   /* Write to server */
	}
	return 0;
}






