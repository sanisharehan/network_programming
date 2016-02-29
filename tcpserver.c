/* Writing here the iterative connection oriented server */
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

int
passiveSock(const char *service, const char *transport, int qlen) {
	struct servent *pserv ;
	struct protoent *pproto ;
	struct sockaddr_in sin ;
	int sd, type;

	/* Initialize sock address struct to 0 */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY ;    //Get connection from any IP address

	/* Map the input service to port number */
	if (pserv = getservbyname(service, transport)) {
		sin.sin_port = (unsigned short)pserv->s_port ;
	}
	else if ( (sin.sin_port = htons( (unsigned short) atoi(service))) == 0) {
		printf("Error in getting service port number.\n");
	}

	/* Map the transport protocol type to port number */
	if ((pproto = getprotobyname(transport)) == 0)
		printf("Error in getting protocol port number. \n");

	/* Use protocol to choose socket type */
	if (strcmp(transport, "udp"))
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	/* Allocate a socket */
	sd = socket(PF_INET, type, pproto->p_proto);
	if(sd < 0)
		printf("Error in allocating a socket \n");

	/* Bind a socket */
	if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		printf("Error in binding the socket to port %s\n", service);

	if (type == SOCK_STREAM && listen(sd, qlen) < 0)
		printf("Can't listen to port \n");
	
	return sd;
}

int passiveTCP(const char *service) {
	return passiveSock(service, "tcp", 0);
}

int main(int argc, char *argv[]){
	return 0;
}
