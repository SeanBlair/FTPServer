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



#define BACKLOG 10      // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once




// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void listening(int socket_fd) {
    int tcpfd, numbytes;

    char buf[MAXDATASIZE];
    char str[MAXDATASIZE];

    socklen_t sin_size;

    struct sockaddr_storage their_addr; // connector's address information
    
    char s[INET6_ADDRSTRLEN];
    
    if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }


    printf("CSftp: waiting for connections...\n");
    printf("next line..\n");


    sin_size = sizeof their_addr;
    tcpfd = accept(socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (tcpfd == -1) {
        perror("accept");
        //continue;  //might need to put this code in loop...
    }

    inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s);
    printf("server2: got connection from %s\n", s);

        // inet_ntop(their_addr.ss_family,
        //     get_in_addr((struct sockaddr *)&their_addr),
        //     s, sizeof s);
        // printf("CSftp: got connection from %s\n", s);
    printf("About to send 230\n");

    if (send(tcpfd, "230 User logged in, proceed :)\n", 30, 0) == -1) {
        printf("error when sending 230\n");
        perror("send");
    }

    printf("sent 230 without error\n");


    printf("Outside while loop..\n");

    while(1) {  // main accept() loop

        printf("Inside while loop..\n");

                // reading client message
        if ((numbytes = recv(tcpfd, buf, MAXDATASIZE-1, 0)) == -1) {

            printf("Inside recv and numbytes == -1..\n");
            
            perror("recv");
            exit(1);
        }

        buf[numbytes] = '\0';

        // printing client message #1
        printf("CSftp: received %s",buf);

        memset(&buf[0], 0, sizeof(buf));

    
        if (send(tcpfd, "500 I will keep answering... i'm a machine :(", 46, 0) == -1) {
            perror("send");
        }    
    }
}



// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

int main(int argc, char **argv) {

    
    struct addrinfo hints, *servinfo, *p;  // not sure what these for... look in tutorial.
    int yes=1;
    int sockfd, new_fd;

    socklen_t sin_size;


    char s[INET6_ADDRSTRLEN];

    struct sockaddr_storage their_addr; // connector's address information

    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc

    int rv, numbytes;   // return value?

    char buf[MAXDATASIZE];
    char *line = NULL;
    size_t size;

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
    // should maybe extract all following code to helper titled listening();
    // with all required parameters...

    listening(sockfd);




    // if (listen(sockfd, BACKLOG) == -1) {
    //     perror("listen");
    //     exit(1);
    // }


    // printf("CSftp: waiting for connections...\n");


    // while(1) {  // main accept() loop
    //     sin_size = sizeof their_addr;
    //     new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    //     if (new_fd == -1) {
    //         perror("accept");
    //         continue;
    //     }

    //     inet_ntop(their_addr.ss_family,
    //         get_in_addr((struct sockaddr *)&their_addr),
    //         s, sizeof s);
    //     printf("CSftp: got connection from %s\n", s);

    //     if (send(new_fd, "230 User logged in, proceed.\n", 30, 0) == -1) {
    //         perror("send");
    //     }


    //     // reading client message #1
    //     if ((numbytes = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1) {
    //         perror("recv");
    //         exit(1);
    //     }

    //     buf[numbytes] = '\0';

    //     // printing client message #1
    //     printf("CSftp: received %s",buf);

    //     // read from console... 
    //     //char *line = NULL;
    //     if (getline(&line, &size, stdin) == -1) {
    //         printf("No line\n");
    //     } else {
    //         // send console message to client
    //         char str[MAXDATASIZE];
    //         strcpy(str, "500 ");
    //         strcat(str, line); 
    //         if (send(new_fd, str, (strlen(line) + 4), 0) == -1) {
    //         perror("send");
    //         }
    //     }

    // }




    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection
    
    //printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;

}
