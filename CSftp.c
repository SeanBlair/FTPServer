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



#define BACKLOG 10      // how many pending connections queue will hold TODO (look into)
#define MAXDATASIZE 100 // max number of bytes we can get at once  TODO (look into)




// the message state of each thread
// returns when "quit"  ??
void messageState(int fd) {

    int numbytes;

    char buf[MAXDATASIZE];
    char str[MAXDATASIZE];

    if (send(fd, "220 Service ready for new user. :)\n", 36, 0) == -1) {
        perror("send");
    }

    while(1) {  // main accept() loop

        printf("CSftp: in message state\n");

                // reading client message
        if ((numbytes = recv(fd, buf, MAXDATASIZE-1, 0)) == -1) {

            printf("Inside recv and numbytes == -1..\n");
            
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        printf("CSftp: received:  %s",buf);

        // it is clearing or flushing buffer (i think)
        // TODO: look into if required or not 
        //memset(&buf[0], 0, sizeof(buf));

        strcpy(str, "500 I will keep answering... i'm a machine :(\n");

        if (send(fd, str, 46, 0) == -1) {
            perror("send");
        }

        printf("CSftp: sent    :  %s",str);
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
