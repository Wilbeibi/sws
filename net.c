/* $$ net.c
 *  
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "net.h"
#include "parse.h"

#define DEBUG 1

static void child_handler(int sig);


int server_listen( Arg_t *optInfo){
    struct addrinfo hints, *res;
    char ipstr[INET_ADDRSTRLEN];
    int listenfd, connfd;
    struct sockaddr_storage client_addr;
    socklen_t clientlen;
    int ignore = 1;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; /* Should support IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; 
    if(optInfo->ipAddr == NULL){
		hints.ai_flags = AI_PASSIVE;
		Getaddrinfo(NULL, optInfo->port, &hints, &res);
    }
    else{
		Getaddrinfo(optInfo->ipAddr, optInfo->port, &hints, &res);
    }
    
    listenfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    /* Avoid Address already in used error */
    Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &ignore, sizeof(int));
    Bind(listenfd, res->ai_addr, res->ai_addrlen);
    Listen(listenfd, LISTENQ);

    while(1){
		pid_t childpid;
		struct sockaddr_in* cliAdr = (struct sockaddr_in*)&client_addr;
		int ipAddr = cliAdr->sin_addr.s_addr;
		clientlen = sizeof(client_addr);
		connfd = Accept(listenfd, (struct sockaddr *) &client_addr, &clientlen);
		printf ("Server is up on port:%s.\n", optInfo->port);
	
		/* Communicate with client */
		if( (childpid = fork()) == 0){
			Close(listenfd);
			Inet_ntop(AF_INET, &ipAddr, ipstr, INET_ADDRSTRLEN); /* Get client address in ipstr */
			
			// while(Read(connfd, readbuf, SIZE) > 0){
			// 	printf("Client %s --> %s", ipstr, readbuf);
			// 	bzero(readbuf, SIZE);
			// }
			read_sock(connfd,optInfo);
			
			exit(0);
		}else {
			Signal(SIGCHLD, child_handler);
		}
		Close(connfd);
    }
    freeaddrinfo(res);
    return connfd;
}



static void child_handler(int sig){
    pid_t p;
    int status;
    while( (p = waitpid(-1, &status, WNOHANG)) != -1)
		;
}



