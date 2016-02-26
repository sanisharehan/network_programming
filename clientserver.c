#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
/* Get host IP address */
struct hostent *host = gethostbyname("localhost");
struct in_addr **host_addr ;
if (host) {
	int i ;
	printf("The host name is %s\n", host->h_name);
	host_addr = (struct in_addr **) host->h_addr_list;
	//for (i = 0; host_addr[i] != NULL; i++)
		//printf("The host IP address is: %d\n", inet_addr(*host_addr[i]));
}
else
{
	herror("gethostbyname");
}

/* Get service port number */
struct servent *serv = getservbyname("daytime","tcp");
if (serv) {
	printf("The service name is %s\n", serv->s_name);
	printf("Network Service port number is %d\n", serv->s_port);
	printf("Host Service port number is %d\n", ntohs(serv->s_port));
}

/* Get protocol port number */
struct protoent *proto = getprotobyname("tcp");
if(proto) {
	printf("The protocol port number for %s protocol is %d\n", "tcp", proto->p_proto);
} 

/* Create a socket */
int sd ;

sd = socket(PF_INET, SOCK_STREAM, proto->p_proto);
if (sd > 0)
	printf ("The socket desc number is %d \n", sd);
else {
	printf("ERROR\n");
	return 1;
}

/* Connect to the socket and read the data */
/* Step 1: Get IP address of host */
struct sockaddr_in sock_in;
memset(&sock_in, 0, sizeof(sock_in));
memcpy(&sock_in.sin_addr, host->h_addr, host->h_length);

/* Step 2: Get port number */
sock_in.sin_port = serv->s_port ;

printf("Addr : %i\n", inet_ntoa(sock_in.sin_addr.s_addr));

int conn = connect(sd, (struct sockaddr *)&sock_in, sizeof(sock_in));
if (conn < 0) {
	printf ("Cant connect to the host.\n");
} else {
	printf("Connection success.\n");
}
return 0;
}
