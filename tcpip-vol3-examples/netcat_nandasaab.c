/* Author: Gaurav Nanda
 * 
 * This is client program which reads data from 
 * its stdin, connects to a given server and 
 * sends data to server. Once it receives EOF, it shuts down.
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int	errexit(const char *format, ...);
int	connectTCP(const char *host, const char *service);

#define	BUFSIZE	100

int main(int argc, char* argv[]) {
    char *host;
    char *port;
	switch (argc) {
	case 3:
		host = argv[1];
		port = argv[2];
		break;
	default:
		fprintf(stderr, "usage: TCPdaytime [host] [port]\n");
		exit(1);
	}
	printf("%s %s\n", host, port);
    /* Actual logic for reading in data from stdin
     * and sending it out to server.
     */
    int s;
    s = connectTCP(host, port);
	
	char buf[BUFSIZE+1];
	int outchars;
	
	/* Read in data from stdin and write to socket. */
	while(1) {
		int bytesread = fread(buf, 1, BUFSIZE, stdin);
		if (bytesread > 0) {
		  write(s, buf, bytesread);
		} else {
			break;
        }
	}
    return 0;
}
