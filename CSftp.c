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

#define BACKLOG 40      // how many pending connections queue will hold, set higher than needed..
#define MAXDATASIZE 100 // max number of bytes we can get at once  TODO (look into)


// returns true if the given command takes no arguments
bool isZeroArgs(char * comm) {
    return (strncmp(comm, "QUIT", 4) == 0) || 
            (strncmp(comm, "PASV", 4) == 0) || 
            (strncmp(comm, "NLST", 4) == 0);
}

// returns true if the given command takes one argument 
bool isOneArgs(char * comm) {
    return (strncmp(comm, "USER", 4) == 0) || 
            (strncmp(comm, "TYPE", 4) == 0) || 
            (strncmp(comm, "STRU", 4) == 0) ||
            (strncmp(comm, "MODE", 4) == 0) || 
            (strncmp(comm, "RETR", 4) == 0);
}

// returns true if the given comm takes the given argument count
bool isCorrectArgCount(char * comm, int count) {
    if (isZeroArgs(comm)) {
        return count == 0;
    } else {
        return count == 1;
    }
}

// returns true if command requires to be logged in
bool needsLogin(char * comm) {    
    return !((strncmp(comm, "QUIT", 4) == 0) || 
        (strncmp(comm, "USER", 4) == 0));
}

// returns true if is valid and supported command
// Zero argument commands are not accepted with added space, 
// 1 argument commands are accepted with or without a space.
bool isValidCommand(char * comm, int totalMessageLength) {
    if (totalMessageLength == 6) {
        return isZeroArgs(comm) || isOneArgs(comm);
    } 
    else {
        return isOneArgs(comm) && comm[4] == ' ';
    }
}


// the message state of each thread
// TODO returns when "quit"  ??
void * messageState(void * socket_fd) {

    int tcpfd;

    // TODO rename?
    socklen_t sin_size;

    struct sockaddr_storage their_addr; // connector's address information
    


    int numbytes, i;
    // incoming message vessel
    char buf[MAXDATASIZE];
    // current total message string length
    int bufStringLength;
    // response message vessel
    char response[MAXDATASIZE];
    // command received : for holding client command. Ex: "USER" or "USER " ('\0' at end)   
    char command[6];   
    // for argument (total incoming message, without the first 5 characters)
    char argument[MAXDATASIZE];
    // do not support arguments with spaces, as 
    // argument with spaces deemed 2 which is currently unsuported... 
    // TODO will this be sufficient?
    int argumentCount;
    // true if have successfully called "USER cs317" previously
    bool isLoggedIn = false;



    // TODO this is where Acton reccomended to start the thread...
    sin_size = sizeof their_addr;
    tcpfd = accept( *(int*) socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (tcpfd == -1) {
        perror("accept");
        //continue;  // TODO: might need to put this code in loop to use the given continue...??
    }




    if (send(tcpfd, "220 Service ready for new user.\n", 33, 0) == -1) {
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
        if ((numbytes = recv(tcpfd, buf, MAXDATASIZE-1, 0)) == -1) {            
            perror("recv");

            // TODO: What to do here... send message to client???
            exit(1);
        }

        // convert read message into string...
        buf[numbytes] = '\0';

        printf("CSftp: received:  %s",buf);

        // parse first 4 characters of buf.
        // force them to upper case
        for (i = 0; i < 4; i++) {
            command[i] = toupper( buf[i]);
        }
        // has argument most likely
        if (buf[4] == ' ') {
            command[4] = ' ';
            command[5] = '\0';  // "USER "
        } else {
           command[4] = '\0';   // "USER"
        }

        printf("CSftp: received the following command: [%s]\n",command);

        
        // parse the argument

        bufStringLength = strlen(buf);

        //  minimum length with arg is 9: "USER x" plus '\0', '\t', '\n' 
        if (bufStringLength < 8) {
            argumentCount = 0;
        } else {
            // argument = buf minus first 5 characters "USER "
            strncpy(argument, buf + 5, MAXDATASIZE);
            // no spaces in string, therefore only one argument
            if (strchr(argument, ' ') == NULL) {
                argumentCount = 1;
            } else {
                // this is equivalent to error, because no command takes 2 args
                // or could catch an illegal second space after command
                argumentCount = 2;
            }
        }


        printf("The provided command: %s has %d number of arguments\n", command, argumentCount);      


        // command recognizer/validator
        // need to rethink and possibly refactor. getting messy.

        if (!isValidCommand(command, bufStringLength)) {
            strcpy(response, "500 Syntax error, command unrecognized. (!isValidCommand)\n");
        }
        
        else if (!isCorrectArgCount(command, argumentCount)) {
            strcpy(response, "501 Syntax error in parameters or arguments. (!isCorrectArgCount)\n");
        }
        
        else if (strncmp(command, "QUIT", 4) == 0) {
            strcpy(response, "221 Service closing control connection.\n"); 
            // TODO return?? with error message??? 
            // or simply return and allow method to continue 
        }
        
        else if (strncmp(command, "USER", 4) == 0) 
        {       
            if (isLoggedIn) 
            {
                strcpy(response, "230 User logged in, proceed.\n");
            }
            else {
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
        } 
        
        else if (needsLogin(command) && !isLoggedIn) {
            strcpy(response, "530 Not logged in.\n");    
        }
        
        else if ((strncmp(command, "TYPE", 4) == 0)) 
        {
            char arg =  argument[0];

            // one character argument followed by CRLF
            if (strlen(argument) == 3) {
                if((arg == 'I') || (arg == 'A')) {
                    strcpy(response, "200 Command okay.\n");    
                }
                else if ((arg == 'L') || (arg == 'E')) {
                    strcpy(response, "504 Command not implemented for that parameter.\n");
                } else {
                    strcpy(response, "501 Syntax error in parameters or arguments.\n");
                }
            } else {
                strcpy(response, "501 Syntax error in parameters or arguments.\n");
            }
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

        if (send(tcpfd, response, MAXDATASIZE, 0) == -1) {
            perror("send");
            // TODO:    What to send client???
        }

        printf("CSftp: sent    :  %s",response);
    }
}


// the listening state of each thread 
// no TCP connection with client yet.
void * listening(void * socket_fd) {      // int socket_fd
    
    
    if (listen( *(int*) socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("CSftp: in listen state.\n");


    

    // // TODO this is where Acton reccomended to start the thread...
    // sin_size = sizeof their_addr;
    // tcpfd = accept( *(int*) socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    // if (tcpfd == -1) {
    //     perror("accept");
    //     //continue;  // TODO: might need to put this code in loop to use the given continue...??
    // }

    // TODO start 4 threads
    messageState(socket_fd);

    // TODO
    // do something awesome when previous returns?
    // could return numbers signifying success or errors//
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

    //TODO  set timeout...
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
    // maybe/maybe not where thread should be started.
    listening(&sockfd);

    return 0;

}
