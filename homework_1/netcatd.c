/* Author: Sanisha Rehan
*
* netcatd.c: Iterative TCP server implementation. The server reads data from
* client and displays to the stdout. The implementation is similar to netcat
* service.
*/

#define	_USE_BSD
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/errno.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define	QLEN		1	/* maximum connection queue length	*/
#define BUFSIZE		1	/* Size of buffer */

int passiveTCP(const char *service, int qlen);
int errexit(const char *format, ...);
extern int errno;

/*-----------------------------------------------------------------------------
* main : Program to implement iterative TCP server for only one connection in
* qlen
* ---------------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	char *service;						/* service name or port number	*/
	struct	sockaddr_in client_sin;		/* the address of a client	*/
	unsigned int alen;					/* length of client's address	*/
	
	int sd, slave_sock;	
	int in_chars;
	char buf[BUFSIZE+1];
    char ip_address[INET_ADDRSTRLEN];

	switch(argc) {
	case 2:
		service = argv[1];
		break;
	default:
		fprintf(stderr, "Usage error: netcatd port");
	}

	/* Connect and bind the socket to a service port */
	sd = passiveTCP(service, QLEN);

	/* Accept the incoming connection */
	alen = sizeof(client_sin);
	slave_sock = accept(sd, (struct sockaddr *) &client_sin, &alen);

	if (slave_sock < 0) {
		if (errno == EINTR)
			printf("Interrupt");
		else
			errexit("accept: %s\n", strerror(errno));
	}

	/* 1.To print client's IP address and port number on stderr */   
	inet_ntop(AF_INET, &(client_sin.sin_addr), ip_address, INET_ADDRSTRLEN);	
	fprintf(stderr, "Client IP address is %s and port number input is %s \n", 
		ip_address, service);

	/* 2.Read data from the connection and write to stdout */
	while ((in_chars = read(slave_sock, buf, BUFSIZE))) {
		if (in_chars < 0) {
			errexit("Netcat Read failed due to: %s\n", strerror(errno));
		}
		fwrite(buf, 1, in_chars, stdout);	
	}

	/* 3.Close sockets once EOF is received */
	close(slave_sock);
	close(sd);
}

