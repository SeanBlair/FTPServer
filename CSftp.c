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


bool isZeroArgs(char * comm) {
    return (strncmp(comm, "QUIT", 4) == 0) || 
    (strncmp(comm, "PASV", 4) == 0) || 
    (strncmp(comm, "NLST", 4) == 0);
}

bool hasWrongNumberArgs(char * comm, int count) {
    if (isZeroArgs(comm)) {
        return count != 0;
    } else {
        return count != 1;
    }
}

bool needsLogin(char * comm) {
    
    return !((strncmp(comm, "QUIT", 4) == 0) || (strncmp(comm, "USER", 4) == 0));
}

bool isValidCommand(char * comm, int messageLength) {
    if (messageLength == 6) {
        return isZeroArgs(comm);
    } 
    else {
        return (strncmp(comm, "USER ", 5) == 0) || 
        (strncmp(comm, "TYPE ", 5) == 0) || 
        (strncmp(comm, "MODE ", 5) == 0) ||
        (strncmp(comm, "STRU ", 5) == 0) || 
        (strncmp(comm, "RETR ", 5) == 0); 
    }
}




// the message state of each thread
// returns when "quit"  ??
void messageState(int fd) {

    int numbytes, i;
    // incoming message vessel
    char buf[MAXDATASIZE];
    int bufStringLength;
    char response[MAXDATASIZE];
    char command[6];  // for holding client command like "USER"
    char argument[MAXDATASIZE];
    int argumentCount;
    bool isLoggedIn = false;
    //bool isValidCommand = false;


    // TODO: what to send first?
    if (send(fd, "220 Service ready for new user.\n", 33, 0) == -1) {
        perror("send");
    }

    while(1) {  // main accept() loop

        // flush response array after previous send.
        memset(&response[0], 0, sizeof(response));

        // flush argument array after previous send.
        memset(&argument[0], 0, sizeof(argument));

        argumentCount = 0;

        printf("CSftp: in message state\n");

                // reading client message
        if ((numbytes = recv(fd, buf, MAXDATASIZE-1, 0)) == -1) {            
            perror("recv");

            // TODO: What to do here... send message to client???
            exit(1);
        }

        // convert read message into string...
        buf[numbytes] = '\0';

        printf("CSftp: received:  %s",buf);

        //printf("The received string is %u character long. \n", (unsigned) strlen(buf));

        // parse first 4 characters of buf.
        // force them to upper case
        for (i = 0; i < 4; i++) {
            command[i] = toupper( buf[i]);
        }
        if (buf[4] == ' ') {
            command[4] = buf[4];
            command[5] = '\0';
        } else {
           command[4] = '\0';
        }

        printf("CSftp: received the following command: [%s]\n",command);

        // TODO: parse the argument as well.

        bufStringLength = strlen(buf);

        if (bufStringLength < 9) {
            argumentCount = 0;
        } else {
            strncpy(argument, buf + 5, MAXDATASIZE);
            if (strchr(argument, ' ') == NULL) {
                argumentCount = 1;
            } else {
                argumentCount = 2;
            }
        }

        // if (bufStringLength < 6) {
        //     //isValidCommand = false;
        //     printf("isValidCommand is false because bufStringLength < 6\n");
        // }
        // // quit s
        // else if (bufStringLength < 9) {
        //     argumentCount = 0;
        //     //isValidCommand = bufStringLength == 6; //isZeroArgs(command);

        //     //printf("isValidCommand = bufStringLength == 6\n");

        // } else {

        //     //isValidCommand = (buf[4] == ' ');

        //     //printf("isValidCommand is buf[4] == ' '\n");

        //     if (isValidCommand) {
        //         // argument is buf minus the command
        //         strncpy(argument, buf + 5, MAXDATASIZE + 5);
        //         if (strchr(argument, ' ') != NULL) {
        //             argumentCount = 2;
        //         } else {
        //             if (strlen(argument) > 3) {
        //                 argumentCount = 1;
        //             } else {
        //                 argumentCount = 0;
        //             }
        //         }
        //     }
        // }

        printf("The provided command: %s has %d number of arguments\n", command, argumentCount);      

        // command recognizer/validator
        // need to rethink and possibly refactor. getting messy.

        if (!isValidCommand(command, bufStringLength)) {
            strcpy(response, "500 Syntax error, command unrecognized. (!isValidCommand)\n");
        }
        
        else if (hasWrongNumberArgs(command, argumentCount)) {
            strcpy(response, "501 Syntax error in parameters or arguments.\n");
        }
        else if (strncmp(command, "QUIT", 4) == 0) {
            strcpy(response, "221 Service closing control connection.\n"); 
        }
        else if (strncmp(command, "USER", 4) == 0) 
        {         
            int argumentLength = strlen(argument);

            // this checks for equality of first 5 characters, 
            // and that there were no more characters other than CR LF.
            if ((strncmp(argument, "cs317", 5) == 0) && (strlen(argument) == 7)) {
                isLoggedIn = true;
                strcpy(response, "230 User logged in, proceed.\n");    
            }
            else {
                strcpy(response, "332 Need account for login.\n");
            }            
        } 
        else if (needsLogin(command) && !isLoggedIn) {
            strcpy(response, "530 Not logged in.\n");    
        }
        else if ((strncmp(command, "TYPE", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "MODE", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "STRU", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "RETR", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "PASV", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }
        else if ((strncmp(command, "NLST", 4) == 0)) 
        {
            strcpy(response, "200 Command okay.\n"); 
        }     
        else {
            strcpy(response, "500 Syntax error, command unrecognized.\n");
        }

        if (send(fd, response, MAXDATASIZE, 0) == -1) {
            perror("send");
            // TODO:    What to send client???
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
