#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"

///////  from demo server  ////////
#include <string.h>
#include <netdb.h>  
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

// my addition
#include <stdbool.h>
#include <ctype.h>

#define BACKLOG 10      // how many pending connections queue will hold TODO (look into)
#define MAXDATASIZE 100 // max number of bytes we can get at once  TODO (look into)


// the message state of each thread
// returns when "quit"  ??
void messageState(int fd) {

    int numbytes, i;

    char buf[MAXDATASIZE];
    char response[MAXDATASIZE];
    char command[6];  // for holding client command like "USER "


    // TODO: what to send first?
    if (send(fd, "220 Service ready for new user. :)\n", 36, 0) == -1) {
        perror("send");
    }

    while(1) {  // main accept() loop

        // this required to flush response array after previous send.
        memset(&response[0], 0, sizeof(response));

        printf("CSftp: in message state\n");

                // reading client message
        if ((numbytes = recv(fd, buf, MAXDATASIZE-1, 0)) == -1) {            
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        printf("CSftp: received:  %s",buf);

        // parse first 5 characters of buf.
        // force them to upper case
        // if then else on them because switching might not work...
        for (i = 0; i < 5; i++) {
            command[i] = toupper( buf[i]);
        }

        // terminate string in order to call strncmp();
        command[5] = '\0';

        printf("CSftp: received the following command:  %s\n",command);

        // command recognizer/validator
        if (strncmp(command, "USER ", 5) == 0) 
        {
            strcpy(response, "200 Command okay.\n");
        } 
        else if ((strncmp(command, "QUIT ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n");
        }
        else if ((strncmp(command, "TYPE ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "MODE ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "STRU ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "RETR ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "PASV ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "NLST ", 5) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }     
        else {
            strcpy(response, "500 Syntax error, command unrecognized.\n");
        }

        if (send(fd, response, MAXDATASIZE, 0) == -1) {
            perror("send");
        }

        printf("CSftp: sent    :  %s",response);
    }
}


// the listening state of each thread 
// no TCP connection with client yet.
void * listening(void * socket_fd) {      // int socket_fd
    
    int tcpfd;

    socklen_t sin_size;

    struct sockaddr_storage their_addr; // connector's address information
    
    if (listen( *(int*) socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("CSftp: in listen state.\n");
    
    sin_size = sizeof their_addr;
    tcpfd = accept( *(int*) socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (tcpfd == -1) {
        perror("accept");
        //continue;  //TODO: might need to put this code in loop to use the continue...
    }

    messageState(tcpfd);

    // do something awesome when previous returns?
}



int main(int argc, char **argv) {
    
    struct addrinfo hints, *servinfo, *p;  // not sure what these for... look in tutorial.
    int yes=1;   
    int sockfd;

    int rv;   // rv == returnValue

    // Check the command line arguments
    if (argc != 2) {
      usage(argv[0]);
      return -1;
    }

    // Create socket with given port number

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "CSftp: failed to bind\n");
        exit(1);
    }

    // listen call..
    // maybe where thread should be started.
    listening(&sockfd);

    return 0;

}
