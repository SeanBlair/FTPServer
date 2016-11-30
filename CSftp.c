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


// from stack overflow
char * replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos){
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return str;
}

// returns true if the given command takes no arguments
bool isZeroArgs(char * comm) 
{
    return (strncmp(comm, "QUIT", 4) == 0) || 
        (strncmp(comm, "PASV", 4) == 0) || 
        (strncmp(comm, "NLST", 4) == 0);
}

// returns true if the given command takes one argument 
bool isOneArgs(char * comm) 
{
    return (strncmp(comm, "USER", 4) == 0) || 
            (strncmp(comm, "TYPE", 4) == 0) || 
            (strncmp(comm, "STRU", 4) == 0) ||
            (strncmp(comm, "MODE", 4) == 0) || 
            (strncmp(comm, "RETR", 4) == 0);
}

// returns true if the given command requires the given argument count
bool isCorrectArgCount(char * comm, int count) {
    if (isZeroArgs(comm)) 
    {
        return count == 0;
    }
    // allow argument TYPE to provide arguments with spaces
    // this is to support TYPE A N
    else if((strncmp(comm, "TYPE", 4) == 0) && ((count == 1) || (count == 2)))
    {
        return true;
    } 
    // all else require one argument
    else
    {
        return count == 1;
    }
}

// returns true if command requires to be logged in
// requires valid commands, precede by check to isValidCommand(comm);
bool needsLogin(char * comm) 
{    
    return !((strncmp(comm, "QUIT", 4) == 0) || 
        (strncmp(comm, "USER", 4) == 0));
}

// returns true if is valid and supported command
// Zero argument commands are not accepted with added space, 
// 1 argument commands are accepted with or without a space.
bool isValidCommand(char * comm, int totalMessageLength) 
{
    if (totalMessageLength == 6) 
    {
        return isZeroArgs(comm) || isOneArgs(comm);
    } 
    else 
    {
        return isOneArgs(comm) && comm[4] == ' ';
    }
}


// the message state of each thread
// TODO returns when "quit"  ??
void * messageState(void * socket_fd) {
    // this TCP connection file descriptor
    int tcpfd, datasockfd, datatcpfd ;

    // used for accept() call
    socklen_t sin_size;

    // used for accept() call
    struct sockaddr_storage their_addr; // connector's address information
    
    // numbytes = bytes received in buf; 
    // i = index of buf[] for parsing
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
    // 0, 1 or 2
    int argumentCount;
    // true if have successfully called "USER cs317" previously
    bool isLoggedIn = false;
    // true if last call was a successful PASV
    bool isDataConnected = false;

    // the IP address of the machine CSftp run on
    // Ex: "87.6.0.6"
    char * myIp;


    // pull off first queued TCP connection from listening socket
    sin_size = sizeof their_addr;
    tcpfd = accept( *(int*) socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (tcpfd == -1) 
    {
        perror("accept");
        // TODO
        // Does it simply wait until there is a connection???
        // Timeout implications??
    }


    // for finding the IP address of the machine running CSftp.o
    struct sockaddr_in sa;
    int sa_len;
    sa_len = sizeof(sa);

    if (getsockname(tcpfd, (struct sockaddr * ) &sa, &sa_len) == -1) 
    {
    perror("getsockname() failed");
    }

    printf("Local IP address is: %s\n", inet_ntoa(sa.sin_addr));
    printf("Local port is: %d\n", (int) ntohs(sa.sin_port));

    myIp = inet_ntoa(sa.sin_addr); 
    printf("myIp is %s\n", myIp);


    // Welcome message to FTP client
    if (send(tcpfd, "220 Service ready for new user.\n", 32, 0) == -1) 
    {
        perror("send");
    }

    // main accept() loop
    while(1) 
    {  
        // flush response array after previous send.
        memset(&response[0], 0, sizeof(response));

        // flush argument array after previous send.
        memset(&argument[0], 0, sizeof(argument));

        // reset
        argumentCount = 0;

        printf("CSftp: in message state\n");


        // reading client message
        if ((numbytes = recv(tcpfd, buf, MAXDATASIZE-1, 0)) == -1) 
        {            
            perror("recv");

            // TODO: What to do here... send message to client???
            exit(1);
        }

        // convert read message into string...
        buf[numbytes] = '\0';

        printf("CSftp: received:  %s",buf);

        // parse command (first 4 characters of buf).
        // force them to upper case
        for (i = 0; i < 4; i++) 
        {
            command[i] = toupper( buf[i]);
        }

        // check next character for ' ' because it implies that 
        // user sent an argument. Used for validating commands.
        // Commands that take arguments are accepted
        // with or without a space after, commands that
        // take 0 arguments are not accepted with space after.
        if (buf[4] == ' ') 
        {
            command[4] = ' ';
            command[5] = '\0';  // "USER " 
        } 
        else 
        {
           command[4] = '\0';   // "USER"
        }

        printf("CSftp: received the following command: [%s]\n",command);

        
        // parse the argument

        bufStringLength = strlen(buf);

        //  minimum length with arg is 7: "USER x" plus '\0'        
        //  (i think CRLF is stripped off by numbytes = recv() -> buf[num[bytes] = '\0')
        if (bufStringLength < 8) 
        {
            argumentCount = 0;
        } else 
        {
            // argument = buf minus first 5 characters "USER "
            strncpy(argument, buf + 5, MAXDATASIZE);
            // no spaces in string, therefore only one argument
            if (strchr(argument, ' ') == NULL) 
            {
                argumentCount = 1;
            } 
            else 
            {
                // this indicates a possible TYPE A N command
                // or could catch an illegal second space after command
                argumentCount = 2;
            }
        }


        printf("The provided command: %s has %d number of arguments\n", command, argumentCount);      


        // command recognizer/validator
        // need to rethink and possibly refactor. getting messy.

        if (!isValidCommand(command, bufStringLength)) 
        {
            strcpy(response, "500 Syntax error, command unrecognized. (!isValidCommand)\n");
        }
        else if (!isCorrectArgCount(command, argumentCount)) 
        {
            strcpy(response, "501 Syntax error in parameters or arguments. (!isCorrectArgCount)\n");
        }
        else if (strncmp(command, "QUIT", 4) == 0) 
        {
            strcpy(response, "221 Service closing control connection.\n"); 

            if (isDataConnected) {
                close(datatcpfd);
                close(datasockfd);
            }  

           if (send(tcpfd, response, MAXDATASIZE, 0) == -1) 
            {
                perror("send");
                // TODO:    What to send client???
            }
            
            close(tcpfd);

            return;
        
        }
        else if (strncmp(command, "USER", 4) == 0) 
        {       
            if (isLoggedIn) 
            {
                strcpy(response, "230 User logged in, proceed.\n");
            }
            else 
            {
                int argumentLength = strlen(argument);

            // this checks for equality of first 5 characters of argument, 
            // and that there were no more characters other than CR LF.
                if ((strncmp(argument, "cs317", 5) == 0) && (strlen(argument) == 7)) 
                {
                    isLoggedIn = true;
                    strcpy(response, "230 User logged in, proceed.\n");    
                }
                else 
                {
                    strcpy(response, "332 Need account for login.\n");
                }    
            }            
        } 
        else if (needsLogin(command) && !isLoggedIn) 
        {
            strcpy(response, "530 Not logged in.\n");    
        }
        else if ((strncmp(command, "TYPE", 4) == 0)) 
        {
            char typeArg =  toupper( argument[0] );

            // one character argument followed by CRLF
            if (strlen(argument) == 3) 
            {
                if(typeArg == 'I') 
                {
                    strcpy(response, "200 Switching to Binary mode.\n");    
                }
                else if (typeArg == 'A') 
                {
                    strcpy(response, "200 Switching to ASCII mode.\n");   
                } 
                else if ((typeArg == 'L') || (typeArg == 'E')) 
                {
                    strcpy(response, "504 Command not implemented for that parameter.\n");
                } 
                else 
                {
                    strcpy(response, "501 Syntax error in parameters or arguments.\n");
                }
            } 
            else 
            {
                strcpy(response, "501 Syntax error in parameters or arguments.\n");
            }
        }
        else if ((strncmp(command, "MODE", 4) == 0)) 
        {
            char modeArg = toupper( argument[0] );
            // one character argument followed by CRLF
            if (strlen(argument) == 3) 
            {
                if(modeArg == 'S') 
                {
                    strcpy(response, "200 Mode set to S.\n");    
                }
                else if ((modeArg == 'B') || (modeArg == 'C')) {
                    strcpy(response, "504 Command not implemented for that parameter.\n");
                } 
                else {
                    strcpy(response, "501 Syntax error in parameters or arguments.\n");
                }
            } 
            else {
                strcpy(response, "501 Syntax error in parameters or arguments.\n");
            }
        }
        else if ((strncmp(command, "STRU", 4) == 0)) 
        {
            char struArg = toupper( argument[0] );
            // one character argument followed by CRLF
            if (strlen(argument) == 3) 
            {
                if(struArg == 'F') 
                {
                    strcpy(response, "200 Structure set to F.\n");    
                }
                else if ((struArg == 'B') || (struArg == 'C')) {
                    strcpy(response, "504 Command not implemented for that parameter.\n");
                } 
                else {
                    strcpy(response, "501 Syntax error in parameters or arguments.\n");
                }
            } 
            else {
                strcpy(response, "501 Syntax error in parameters or arguments.\n");
            } 
        }
        else if ((strncmp(command, "RETR", 4) == 0)) 
        {
            if (isDataConnected)
            // accept() datatcpfd, sendFile() on datatcpfd 
            {
                strcpy(response, "200 (isDataConnected)\n");   
                // do the work
                // close data connection.
                // close datasocket
                isDataConnected = false;
            }
            else
            {
                strcpy(response, "425 Use PASV first.\n"); 
            }
        }
        else if ((strncmp(command, "PASV", 4) == 0)) 
        {
            // need to create socket using port "0" to allow OS to choose unused one. 
            // after call to bind(), port # will be available with getsockname()    
        
            // open socket on this port, and listen for connections.

            // respond with current ip and new port 
            // current ip is not available until after accept(), which will not
            // happen until later. But, when control connection is accept() we can read our IP address
            // with getsockname() and store it as messageState() state.

            // set state.  isDataConnected == true; 

            // each call to RETR or NLST must be preceded by a call to PASV.

            if (isDataConnected)
            {   
                // TODO
                // close existing connection
                close(datatcpfd);
                close(datasockfd);

                // reset all relevant values
                datatcpfd = -1;
                datasockfd = -1;

                isDataConnected = false;
            }


            // create new socket, listen for connections
            // set datasockfd
            // TODO refactor repeated code...

            struct addrinfo hints, *servinfo, *p;
            int yes=1;   
            int rv;   

            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE; // use my IP

            if ((rv = getaddrinfo(NULL, "0", &hints, &servinfo)) != 0) 
            {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            }

            //TODO  set timeout...

            // loop through all the results and bind to the first we can
            for(p = servinfo; p != NULL; p = p->ai_next) 
            {
                if ((datasockfd = socket(p->ai_family, p->ai_socktype,
                        p->ai_protocol)) == -1) 
                {
                    perror("server: socket");
                    continue;
                }
        
                if (setsockopt(datasockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                        sizeof(int)) == -1) 
                {
                    perror("setsockopt");
                    exit(1);
                }

                if (bind(datasockfd, p->ai_addr, p->ai_addrlen) == -1) 
                {
                    close(datasockfd);
                    perror("server: bind");
                    continue;
                }
                break;
            }

            // for checking port number of data socket

            struct sockaddr_in sa;
            int sa_len;
            sa_len = sizeof(sa);

            if (getsockname(datasockfd, (struct sockaddr * ) &sa, &sa_len) == -1) 
            {
                perror("getsockname() failed");
            }

            printf("Local port is: %d\n", (int) ntohs(sa.sin_port));
            // don't need this
            char * myIpString = replace_char(myIp, '.', ',');
            printf("myIpString is: %s\n", myIpString);


            int port = (int) ntohs(sa.sin_port);
            // parse port in FTP pasv connection format
            int a = port / 256;
            char aStr[11];
            sprintf(aStr, "%d", a);
            int b = port % 256;
            char bStr[11];
            sprintf(bStr, "%d", b);
            
            strcpy(response, "227 Entering Passive Mode. (");
            strncat(response, myIpString, 16);
            strncat(response, ",", 1);
            strncat(response, aStr, 3);
            strncat(response, ",", 1);
            strncat(response, bStr, 3);
            strncat(response, ")\n", 2);


            freeaddrinfo(servinfo); // all done with this structure

            if (p == NULL)  
            {
                fprintf(stderr, "CSftp: (data connection) failed to bind\n");
                exit(1);
                // TODO error message??
            }


            // start listening...
            if (listen(datasockfd, BACKLOG) == -1) 
            {
                perror("listen");
                exit(1);
                // TODO error??
            }

            isDataConnected = true; 
        }
        else if ((strncmp(command, "NLST", 4) == 0)) 
        {
            if (isDataConnected) 
            {
                // used for accept() call
                socklen_t data_sin_size;

                // used for accept() call
                struct sockaddr_storage data_their_addr; // connector's address information

                // pull off first queued TCP connection from listening socket
                // TODO if nothing to do??

                data_sin_size = sizeof data_their_addr;
                datatcpfd = accept( datasockfd, (struct sockaddr *)&data_their_addr, &data_sin_size);
                if (datatcpfd == -1) 
                {
                    printf("accept(datasockfd) produced a -1...");
                     perror("accept");
                //continue;  // TODO: might need to put this code in loop to use the given continue...??
                // Does it simply wait until there is a connection???
                // Timeout implications??
                }

                if (send(tcpfd, "150 Here comes the directory listing.\n", 38, 0) == -1) 
                {
                    perror("send");
                    // TODO:    What to send client???
                    // 450 maybe??
                }


                // returns count of files printed
                int filesListed = listFiles(datatcpfd, ".");
                if (filesListed == -1) 
                {
                    printf("\nlistfiles returned -1 (directory does not exist or you don't have permission)\n");
                    perror("listFiles");
                    strcpy(response, "451 Requested action aborted: local error in processing.\n");

                } 
                else if (filesListed == -2) 
                {
                    printf("\nlistfiles returned -2 (insufficient resources to perform request)\n ");
                    strcpy(response, "426 Connection closed; transfer aborted.\n");
                }

                printf("\nfiles listed was %d\n", filesListed);

                // happy case
                strcpy(response, "226 Directory send OK.\n");

                close(datatcpfd);
                close(datasockfd);

                isDataConnected = false;
            }
            else
            {
                strcpy(response, "425 Use PASV first.\n"); 
            } 
        }     
        else 
        {
            strcpy(response, "500 Syntax error, command unrecognized.\n");
        }

        if (send(tcpfd, response, MAXDATASIZE, 0) == -1) 
        {
            perror("send");
            // TODO:    What to send client???
        }
        
        printf("CSftp: sent    :  %s",response);
    }
}


// the listening state of each thread 
void * listening(void * socket_fd) 
{              
    if (listen( *(int*) socket_fd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }

    printf("CSftp: in listen state.\n");


    // TODO this is where Acton reccomended to start the thread...
    // TODO start 4 threads
    messageState(socket_fd);

    return;
    // TODO
    // do something awesome when previous returns?
    // could return numbers signifying success or errors//
    // or could simply return when done execution.
}


// FTP server
int main(int argc, char **argv) 
{
    // some code based on http://beej.us/guide/bgnet/examples/server.c
    
    struct addrinfo hints, *servinfo, *p;
    int yes=1;   
    int sockfd;

    int rv;   

    // Check the command line arguments
    if (argc != 2) 
    {
      usage(argv[0]);
      return -1;
    }

    // Create socket with given port number

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //TODO  set timeout...
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) 
        {
            perror("server: socket");
            continue;
        }
        
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) 
        {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) 
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  
    {
        fprintf(stderr, "CSftp: failed to bind\n");
        exit(1);
    }

    // listen call..
    // maybe/maybe not where thread should be started.
    listening(&sockfd);

    // when QUIT called or errors?
    // TODO figure it out
    close(sockfd);

    return 0;

}
