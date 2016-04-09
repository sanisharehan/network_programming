/* Author: Sanisha Rehan
 * A simple Multi-threaded, concurrent TCP web-server. This server is used
 * to serve HTTP GET requests and provide mapping between url paths to 
 * different file system paths.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define	QLEN		  32	/* maximum connection queue length	*/
#define	BUFSIZE		1024

#define	GETREQUEST	"GET"
#define INDEX		"index.html"
#define IMAGE		"/img/"
#define IMAGEABS	"/images/"
#define CONNCLOSE 	"close"
#define SERVER		"207httpd/0.0.1"
#define MESSAGE404	"<html>Error: 404 Page Not Found </html>"

/* Global and User-defined Variables */
char *rootDirectory;			/* root directory location */

struct reqResponse{
	int statusCode;
	char *status;
	char *server;
	char *connection;
	char *contentType;
	size_t contentLength;
};

int	 errexit(const char *format, ...);
int	 passiveTCP(const char *service, int qlen);
int  TCPWebResponse(int sd);
int  sendResponse(char *inBuf, int sock);
void sendFileContents(int sock, const char *filePath, struct reqResponse *reqResp);
int  getAbsFileLoc(char *inFileLoc, char *absFileLoc);
void initReqResponse(struct reqResponse *reqRes);
void httpWriteline(int sock, char *vPtr, size_t nBytes);
int  httpReadline(int sock, char *buf, int maxlen);
int  httpGetFilesize(const char *fileName);
char * httpGetMime(const char *fileName);

/*------------------------------------------------------------------------
 * main - Concurrent Multi-threaded TCP server for web service
 *------------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
	pthread_t thId;
	pthread_attr_t thAttr;
	char *service;		        	/* service name or port number	*/
	struct sockaddr_in clientAddr;	/* the address of a client	*/
	unsigned int addrLen;			/* length of client's address	*/
	int	mSock;						/* master server socket		*/
	int	sSock;						/* slave server socket		*/

	switch (argc) {
	case 3:
		service = argv[1];
		rootDirectory = argv[2];
		break;
	default:
		errexit("Error in server: Usage 207httpd port root_directory\n");
	}

	mSock = passiveTCP(service, QLEN);

	(void) pthread_attr_init(&thAttr);
	(void) pthread_attr_setdetachstate(&thAttr, PTHREAD_CREATE_DETACHED);
    
	/* Check for incoming connections */
	while (1) {
		addrLen = sizeof(clientAddr);
		sSock = accept(mSock, (struct sockaddr *)&clientAddr, &addrLen);
		if (sSock < 0) {
			if (errno == EINTR)
				continue;
			errexit("accept: %s\n", strerror(errno));
		}
		if (pthread_create(&thId, &thAttr, (void * (*)(void *))TCPWebResponse,
		    (void *)(long)sSock) < 0)
			errexit("pthread_create: %s\n", strerror(errno));
	}
}

/*-----------------------------------------------------------------------------
 * TCPWebResponse - Function to generate the response for each client as per 
 * the request and sends the response.
 * Arguments:	sd: Slave socket descriptor for communicating with client through
 * slave thread.
 *-----------------------------------------------------------------------------
*/
int 
TCPWebResponse(int sd){
	char inBuf[BUFSIZE];
	char ip_address[INET_ADDRSTRLEN];

    socklen_t addrSize = sizeof(struct sockaddr_in);
	struct sockaddr_in sAddr;
	
	/* Step 1: Echo client IP address, port and GET request on stdout */
	getpeername(sd, (struct sockaddr *)&sAddr, &addrSize);
	inet_ntop(AF_INET, &(sAddr.sin_addr), ip_address, INET_ADDRSTRLEN);
	fprintf(stdout, "Client IP address: %s and Port number: %d \n", ip_address, sAddr.sin_port);
	
	httpReadline(sd, inBuf, BUFSIZE);
	fprintf(stdout, "The input request is %s \n", inBuf);
	
	/* Step 2: Send response to the client and terminate the thread */
	sendResponse(inBuf, sd);
	pthread_exit(NULL);
	return 0;
}

/*-----------------------------------------------------------------------------
 * get_response - Function to read the request sent from the client and generate
 * response accordingly. This function performs following tasks:
 * a. Checks for the request type GET/POST
 * b. Looks for the file location
 * c. Generate reponse headers and send to client
 * d. Send the file contents
 *
 * Arguments:	inBuf: GET request from client
 *				sock: Slave socket used for communication with client
-----------------------------------------------------------------------------*/
int
sendResponse(char *inBuf, int sock) {
	int tokenCount = 0;
	char in[BUFSIZE];
	char *tokenArr[4];
	char *tok;
	char *requestType;
	char *fileLoc;
	char absFileLoc[BUFSIZE];
	char headBuf[BUFSIZE];
	char serBuf[100];
	char connBuf[100];
	char contBuf[100];
	char contLenBuf[100];
	struct reqResponse reqRes;	

	strcpy(in, inBuf);
	
	/* Get request tokens */	
	tok = strtok(in, " ");
	while (tok != NULL) {
		tokenArr[tokenCount] = tok;
		tok = strtok(NULL, " ");
		tokenCount++;
	}
	requestType = (char *)malloc(sizeof(char) * (strlen(tokenArr[0]) + 1));
	fileLoc = (char *)malloc(sizeof(char) * (strlen(tokenArr[1]) + 1));	
	strcpy(requestType, tokenArr[0]);
	strcpy(fileLoc, tokenArr[1]);

	/* a. Perform request validation i.e. only GET requests are handled */
	if (strcmp (requestType, GETREQUEST) != 0) {
		fprintf(stderr, "The server supports only GET requests, sent request is %s. \n",
			requestType);
		return -1;
	}

	/* b. Get absolute file path */
	getAbsFileLoc(fileLoc, absFileLoc);
	
	/* c. Generate reponse header */
	initReqResponse(&reqRes);
	if (access(absFileLoc, R_OK) == -1)	{
		/* File does not exist or Read permission is not available */
		reqRes.statusCode = 404;
		reqRes.status = "File Not Found";
		reqRes.contentType = "text/html";
		reqRes.contentLength = strlen(MESSAGE404);
	} else {
		/* File exists */
		reqRes.statusCode = 200;
		reqRes.status = "OK";
		reqRes.contentType = httpGetMime(absFileLoc);
		reqRes.contentLength = httpGetFilesize(absFileLoc);
	}
	
	/* d. Send reponse headers */
	sprintf(headBuf, "HTTP/1.1 %d %s\r\n", reqRes.statusCode, reqRes.status);
	sprintf(serBuf, "Server: %s\r\n", reqRes.server);
	sprintf(connBuf, "Connection: %s\r\n", reqRes.connection);
	sprintf(contBuf, "Content-Type: %s\r\n", reqRes.contentType);
	sprintf(contLenBuf, "Content-Length: %d\r\n", (int) reqRes.contentLength);
	
	httpWriteline(sock, headBuf, strlen(headBuf));
	httpWriteline(sock, serBuf, strlen(serBuf));
	httpWriteline(sock, connBuf, strlen(connBuf));
	httpWriteline(sock, contBuf, strlen(contBuf));
	httpWriteline(sock, contLenBuf, strlen(contLenBuf));
	httpWriteline(sock, "\r\n", 2);

	/* e. Send file contents if the file exists */
	sendFileContents(sock, absFileLoc, &reqRes);
	free(tok);
	return 0;
}

/*-----------------------------------------------------------------------------
 * sendFileContents - Function to write file contents to the socket.
 * Arguments:	sock: Slave socket desc
 *				filePath: Absolute location of file
 * 				reqResp: reqResponse struct pointer corresponding to request
-----------------------------------------------------------------------------*/
void
sendFileContents(int sock, const char *filePath, struct reqResponse *reqResp) {
	char buf[BUFSIZE + 1];
	int bytesCount;
	FILE *fp;

	if (reqResp->statusCode != 200) {
		strcpy(buf, MESSAGE404);
		if (write(sock, buf, sizeof(buf)) < 0)
			errexit("Error writing to connection \n");
		return;
	}
	
	fp = fopen(filePath, "r");	
	while ((bytesCount = fread(buf, sizeof(char), BUFSIZE, fp)) > 0) {
		if (bytesCount < 0) {
			errexit("Error reading from file %s. \n", filePath);
		}
		if (write(sock, buf, bytesCount) < 0) {
			errexit("Error writing to connection \n");
		}
	}
	fclose(fp);
}

/*-----------------------------------------------------------------------------
 * getAbsFileLoc - Function to get the absolute path for the input file from
 * w.r.t root directory.
 * Arguments:	inFileLoc: File path location provided by client
 * 				absFileLoc: Absolute file location w.r.t root directory
-----------------------------------------------------------------------------*/
int 
getAbsFileLoc(char *inFileLoc, char *absFileLoc) {
	char origPath[BUFSIZE];
	char *imgPtr;
	char relFileLoc[BUFSIZE];

	strcpy(relFileLoc, inFileLoc);

	/* i. If location for images is given e.g. /images/sjsu.jpg or 
	/photos/images/sjsu.jpg, replace images with img */
	if ((imgPtr = strstr(inFileLoc, IMAGEABS)) != NULL) {
		/* This gets the path before /images/ */
		strncpy(origPath, inFileLoc, imgPtr - inFileLoc);
		origPath[imgPtr - inFileLoc] = '\0';

		/* Replace the /images/ with /img/ and concatenate file name */
		sprintf(origPath + (imgPtr - inFileLoc), "%s%s", IMAGE, 
			(imgPtr + strlen(IMAGEABS)));
		relFileLoc[0] = '\0';
		strcpy(relFileLoc, origPath);
	}
	
	/* ii. If URL endswith /, index.html is appended */
	if ( *relFileLoc && (relFileLoc[strlen(relFileLoc) - 1] == '/')) {
		strcat(relFileLoc, INDEX);
	}

	/* iii. If none of the above conditions are met, get absolute path by
		concatenating root directory path */
	strcpy(absFileLoc, rootDirectory);
	strcat(absFileLoc, relFileLoc);

	free(imgPtr);
	return 0;
}


/*-----------------------------------------------------------------------------
 * initReqResponse - Function to initialize the request response struct to default
 * values.
-----------------------------------------------------------------------------*/
void
initReqResponse(struct reqResponse *reqRes) {
    reqRes->statusCode = 200;
    reqRes->status     = NULL;
    reqRes->server     = SERVER;
    reqRes->connection = CONNCLOSE;
    reqRes->contentType = NULL;
	reqRes->contentLength = 0;          
}

/*-----------------------------------------------------------------------------
 * httpReadline - Function to read a single line from socket and save it into 
 * a null-terminated string.
-----------------------------------------------------------------------------*/
int
httpReadline(int sock, char *buf, int maxlen) {

	int n = 0;
	char *p = buf;
	
	while (n < maxlen - 1) {
		char c;
		int rc = read(sock, &c, 1);
		if (rc == 1) {
			/* Stop at \n and strip away \r\n */
			if (c == '\r' || c == '\n' ) {
				*p = '\0';		/* null-terminated */
				return n;
			} else if (c != '\n' && c != '\r') {
				*p++ = c;
				n++;
			}
		} else if (rc == 0) {
			return -1;		/* EOF */
		} else if (errno == EINTR) {
			continue;		/* retry */
		} else {
			return -1;		/* error */
		}		
	}
	errexit("Error in httpReadline(): Buffer too small\n");
	return -1;
}

/*-----------------------------------------------------------------------------
 * httpWriteline - Function to write contents to the socket.
-----------------------------------------------------------------------------*/
void
httpWriteline(int sock, char *vPtr, size_t nBytes) {
    size_t      nLeft;
    ssize_t     nWritten;
    char *buffer;

    buffer = vPtr;
    nLeft  = nBytes;

    while (nLeft > 0) {
	if ((nWritten = write(sock, buffer, nLeft)) <= 0 ) {
	    if (errno == EINTR) {
			nWritten = 0;
		}
	    else
			errexit("Error in httpWriteline()\n");
	}
	nLeft  -= nWritten;
	buffer += nWritten;
    }
}

/*-----------------------------------------------------------------------------
 * httpGetFilesize - Function to get the file size.
-----------------------------------------------------------------------------*/
int
httpGetFilesize(const char *fileName) {
	FILE *fp;
	int fSize;

	fp = fopen(fileName, "r");
	if (fp == NULL) {
		errexit("File %s cannot be opened for reading\n", fileName);
	}
	fseek(fp, 0, SEEK_END);
	fSize = ftell(fp);
	rewind(fp);
	fclose(fp);
	return fSize;
}

/*-----------------------------------------------------------------------------
 * httpGetMime - Function to get the media type of the file.
-----------------------------------------------------------------------------*/
char *
httpGetMime(const char *fileName) { 
	const char *extF = strrchr(fileName, '.');
	if (extF == NULL) {
		return "application/octet-stream";
	} else if (strcmp(extF, ".html") == 0) { 
		return "text/html";
	} else if (strcmp(extF, ".jpg") == 0) {
		return "image/jpeg";
	} else if (strcmp(extF, ".gif") == 0) {
		return "image/gif";
	} else {
		return "application/octet-stream";
	}
}
