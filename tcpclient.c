#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef ERR_ADDR
#define ERR_ADDR 0xffffffff
#endif

typedef unsigned short u_short;
extern int errno;

//Function to create and connect socket to the server
int connectSocket(const char *host_name, const char *service_name, const char *transport_name) {
    struct hostent *phost;
    struct servent *pserv;
    struct protoent *pproto;
    struct sockaddr_in sock_addr;
    int sd, type;
    //Set or initialize the memory block for socket address to all 0 values
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;

    /* Get the IP address of host machine */
    if (phost = gethostbyname(host_name)) {
	printf(" The IP address for host name %s is %s", host_name, phost->h_addr);
	memcpy(&sock_addr.sin_addr, phost->h_addr, phost->h_length);
    } else {
	printf ("Could not find host address for given host name %s", host_name);
    }

    /* Get service port number */
    if (pserv = getservbyname(host_name, service_name)) {
	printf("The service port number for given service %s is %d", service_name, pserv->s_port);
	sock_addr.sin_port = pserv->s_port ;
    } else if ((sock_addr.sin_port = htons( atoi(service_name))) == 0) {
	printf("The service port is 0");
    }

    /* Get the protocol port number */
    if ((pproto = getprotobyname(transport_name)) == 0)
	printf("We cannot get the protocol entry");

    /* Choose the protocol type */
    if (strcmp(transport_name, "udp") == 0)
	type = SOCK_DGRAM;
    else if (strcmp(transport_name, "tcp") == 0)
	type = SOCK_STREAM;

    /* Create the socket */
    sd = socket(PF_INET, type, pproto->p_proto);
    if (sd < 0)
	printf ("Cant create the socket due to error %s", strerror(errno));

    /* Allocate the socket to create the connection */
    if (connect (sd, (struct sockaddr *) &sock_addr, sizeof(sock_addr)) < 0 )
	printf ("Cant connect the socket to the server due to error %s", strerror(errno));

    return sd ;
}
	
//Function to connect using TCP
int connectTCP(const char *host_name, const char *service_name) {
	return connectClient(host_name, service_name, "tcp");
}

//Function to connect using UDP
int connectUDP(const char *host_name, const char *service_name) {
	return connectClient(host_name, service_name, "udp");
}

//Main function for program
int main(int argc, char *argv[]) {
  char *host = "localhost";
  char *service = "daytime" ;
  int sd;

  switch(argc) {
  case 1: host = "localhost";
	break;
  case 3:
	service = argv[2]; 
  case 2:
	host = argv[1];
	break ;
  default:
	fprintf(stderr, "No host or service specified");
        exit(1);
  } 
  sd = connectTCP(host, service);
}












