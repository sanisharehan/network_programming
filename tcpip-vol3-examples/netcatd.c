/* Author: Gaurav Nanda */

#define	_USE_BSD
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define	QLEN		  1	/* maximum connection queue length	*/
#define	BUFSIZE	4096

int	TCPnetcatd(int fd);
int	errexit(const char *format, ...);
int	passiveTCP(const char *service, int qlen);

/*------------------------------------------------------------------------
 * main - Single client based netcat server.
 *------------------------------------------------------------------------
 */
int main(int argc, char* argv[])
{
	struct	sockaddr_in fsin;	/* the address of a client	*/
	unsigned int alen;		/* length of client's address	*/

	char *port;
	switch (argc) {
	case 2:
		port = argv[1];
		break;
	default:
		errexit("usage: TCPechod [port]\n");
	}

    int sock;
	sock = passiveTCP(port, QLEN);
    
    alen = sizeof(fsin);
    int fd = accept(sock, (struct sockaddr*)&fsin, &alen);	
	if (fd < 0) {
		errexit("accept: %s\n", strerror(errno));
	}
	/* Actual logic for printing out characters. */

	/* 1) Lets print the client's ServerId and Port No to Stderr. */
	char ip_address[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(fsin.sin_addr), ip_address, INET_ADDRSTRLEN);
    fprintf(stderr, "Client IP: %s and Port no: %u\n", ip_address, fsin.sin_port);

	/* 2) Read data from client and write it to stdout */
	int characters_read;
    char buf[BUFSIZE + 1];
    while (characters_read = read(fd, buf, BUFSIZE)) {
		if (characters_read < 0) {
			errexit("echo read: %s\n", strerror(errno));
		}
	    fwrite(buf, 1, characters_read, stdout);
    }
	close(fd);
    close(sock);
}
