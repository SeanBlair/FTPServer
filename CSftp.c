#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <string.h>
#include <netdb.h>  
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>

#define BACKLOG 40      // how many pending connections queue will hold, set higher than needed..
#define MAXDATASIZE 256 // max number of bytes we can get at once


// based on Stack Overflow thread...
char * replace_character(char* string, char find, char replace){
    char *current_pos = strchr(string,find);

    while (current_pos)
    {
        *current_pos = replace;
        current_pos = strchr(current_pos,find);
    }
    return string;
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

// Returns true if the given command requires the given argument count
// or if not a supported command
bool isCorrectArgCount(char * comm, int count) 
{
    if (isZeroArgs(comm)) 
    {
        return count == 0;
    }
    // all else require one argument
    else if (isOneArgs(comm))
    {
        return count == 1;
    }
    // not suported command case
    else
        return true;
}


// returns true if first 3 characters of comm are each [A-Z, a-z],
// 4th character can be a [A-Z, a-z] or space. If command longer than
// 4 characters, the 5th character must be a space.
bool isValidCommand(char * comm, int bufStringLength) 
{
    bool isValid = true;
    int i;

    // command less than 3 characters
    if (bufStringLength < 5)
    {
        isValid = false;
    }
    else if (bufStringLength >= 5)
    {
        // check if first 3 characters are all [A-Z, a-z]
        for (i = 0; i < 3; i++)
        {
            if (!isalpha(comm[i]))
            {
                isValid = false;
                break;
            }
        }
        // If valid so far, and more than 3 characters, 
        // allow fourth character to be either [A-Z, a-z] or == ' ';
        if (isValid && (bufStringLength >= 6))
        {
            if ( !(isalpha(comm[3]) || (comm[3] == ' ')) )
            {
                isValid = false;
            }   
        }
        // if valid so far and more than 4 characters,
        // check that the 5th character is a space
        if (isValid && (bufStringLength > 6))
        {
            isValid = comm[4] == ' ';
        }
    }
    return isValid;
}


// the message state of each thread
void * messageState(void * socket_fd) {

    // this TCP connection file descriptor
    int tcpfd;
    // the data socket file descriptor
    int datasockfd;
    // the data TCP connection file descriptor
    int datatcpfd;

    // used for accept() call
    socklen_t sin_size;
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
    // command received : for holding client command. Ex: "USER" ('\0' at end)   
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
    sin_size = sizeof (their_addr);
    tcpfd = accept( *(int*) socket_fd, (struct sockaddr *)&their_addr, &sin_size);
    if (tcpfd == -1) 
    {
        perror("accept");
        pthread_exit(NULL);
    }

    // for finding the IP address of the machine running CSftp.o
    struct sockaddr_in sa;
    int sa_len;
    sa_len = sizeof(sa);

    if (getsockname(tcpfd, (struct sockaddr * ) &sa, &sa_len) == -1) 
    {
        perror("getsockname() failed");
        close(tcpfd);
        pthread_exit(NULL);
    }

    printf("Local IP address is: %s\n", inet_ntoa(sa.sin_addr));

    myIp = inet_ntoa(sa.sin_addr); 


    // Welcome message to FTP client
    if (send(tcpfd, "220 Service ready for new user.\n", 32, 0) == -1) 
    {
        perror("send");
        close(tcpfd);
        pthread_exit(NULL);
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
            //exit(1);
        }
        else if (numbytes == 0)
        {
            //retry reading.
            break;
        }

        // convert read message into string.
        buf[numbytes] = '\0';

        // parse command (first 4 characters of buf).
        // force them to upper case
        for (i = 0; i < 4; i++) 
        {
            command[i] = toupper( buf[i]);
        }

        command[4] = '\0';   // "USER"

        printf("CSftp: received the following command: [%s]\n",command);

        
        // parse the argument

        bufStringLength = strlen(buf);

        //  length with no arg is 6: "USER" plus '\0' plus '\n'        
        if (bufStringLength <= 6)
        {
            argumentCount = 0;
        } 
        else 
        {
            // argument = buf minus first 5 characters
            strncpy(argument, buf + 5, MAXDATASIZE - 10);

            // The first character of the argument is neither a space nor a '\n'
            char firstArgChar = argument[0];

            if (((firstArgChar != ' ') || (firstArgChar != '\n')) && (strlen(argument) > 2))  
            {
                argumentCount = 1;
            } 
            else 
            {
                // this could catch an illegal second space after command
                // indicates exception path, to be further examined.
                argumentCount = 2;
            }
        }   


        // command recognizer/validator
        
        if (!isValidCommand(buf, bufStringLength)) 
        {
            strcpy(response, "500 Syntax error, command unrecognized.\n");
        }
        else if (!isCorrectArgCount(command, argumentCount)) 
        {
            strcpy(response, "501 Syntax error in parameters or arguments.\n");
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

            printf("CSftp: sent    :  %s",response);
            
            close(tcpfd);

            // return;
            pthread_exit(NULL);     
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
            // and that there are no more characters.
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
        else if ((strncmp(command, "TYPE", 4) == 0)) 
        {
            char typeArg =  toupper( argument[0] );

            // one character argument
            if (strlen(argument) == 3) 
            {
                if(typeArg == 'I') 
                {
                    if (isLoggedIn)
                    {
                        strcpy(response, "200 Switching to Binary mode.\n");    
                    }
                    else
                    {   
                        strcpy(response, "530 Not logged in.\n");
                    }
                }
                else if (typeArg == 'A') 
                {
                    if (isLoggedIn)
                    {
                        strcpy(response, "200 Switching to ASCII mode.\n");    
                    }
                    else
                    {
                        strcpy(response, "530 Not logged in.\n");   
                    }                   
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
                if (typeArg == 'A') 
                {
                    if (argument[1] == ' ')
                    {
                        char typeSecondArg = toupper(argument[2]);
                        if ((typeSecondArg == 'N') || (typeSecondArg == 'T') || (typeSecondArg == 'C'))
                        {
                            strcpy(response, "504 Command not implemented for that parameter.\n");
                        }
                        else
                        {
                            strcpy(response, "501 Syntax error in parameters or arguments.\n");
                        }
                    }
                }
                else 
                {
                strcpy(response, "501 Syntax error in parameters or arguments.\n");
                }
            }
        }
        else if ((strncmp(command, "MODE", 4) == 0)) 
        {
            char modeArg = toupper( argument[0] );
            // one character argument
            if (strlen(argument) == 3) 
            {
                if(modeArg == 'S') 
                {
                    if (isLoggedIn)
                    {
                        strcpy(response, "200 Mode set to S.\n");        
                    }
                    else
                    {
                        strcpy(response, "530 Not logged in.\n");    
                    }                    
                }
                else if ((modeArg == 'B') || (modeArg == 'C')) 
                {
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
            // TODO: check.
            char struArg = toupper( argument[0] );
            // one character argument followed by CRLF
            if (strlen(argument) == 3) 
            {
                if(struArg == 'F') 
                {
                    if (isLoggedIn)
                    {
                        strcpy(response, "200 Structure set to F.\n");        
                    }
                    else
                    {
                        strcpy(response, "530 Not logged in.\n");       
                    }                   
                }
                else if ((struArg == 'B') || (struArg == 'C')) 
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

        else if ((strncmp(command, "RETR", 4) == 0)) 
        {
            if (isLoggedIn)
            {
                if (isDataConnected)
                {
                    socklen_t rdata_sin_size;
                    struct sockaddr_storage rdata_their_addr;

                    rdata_sin_size = sizeof rdata_their_addr;
                    datatcpfd = accept( datasockfd, (struct sockaddr *)&rdata_their_addr, &rdata_sin_size);
                    if (datatcpfd == -1) 
                    {
                        printf("accept(datasockfd) produced a -1...");
                        perror("accept");
                        //TODO
                    // Timeout implications??
                    }

                    FILE *fp = NULL;

                    char fileName[MAXDATASIZE];
                    char buff[MAXDATASIZE];

                    bzero(fileName, 0);
                    bzero(buff, 0);

                    int  c;

                    char * file;

                    int len = strlen(argument);

                    // argument[len-1] == '\0' (end of string)
                    // but previous character is '\n'.
                    // need to eliminate this character from argument.
                    argument[len-2] = '\0';

                    file = argument;

                    fp = fopen(file,"r");

                    if (fp != NULL)
                    {
                        if (send(tcpfd, "125 Data connection already open; transfer starting.\n", 53, 0) == -1) 
                        {
                            perror("send");
                            // TODO:    What to send client???
                            // 450 maybe??
                        }

                        while(1)
                        {
                            // TODO look into these numbers, they look suspicious, although the functionality is there.
                            c = fread(buff, 1, 52, fp);

                            if ( feof(fp) )
                            { 
                            break ;
                            }
                
                            if (write(datatcpfd, buff, c) == -1) 
                            {
                                perror("write");
                                //exit(EXIT_FAILURE);
                            }
                        }

                        fclose(fp);

                        strcpy(response, "226 Closing data connection. Requested file action successful\n"); 
                    }
                    else
                    {
                        strcpy(response, "550 Requested action not taken. File unavailable\n");
                    }

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
                strcpy(response, "530 Not logged in.\n");
            }
        }
        else if ((strncmp(command, "PASV", 4) == 0)) 
        {
            if (isLoggedIn)
            {

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

                // for getting port number of data socket

                struct sockaddr_in sa;
                int sa_len;
                sa_len = sizeof(sa);

                if (getsockname(datasockfd, (struct sockaddr * ) &sa, &sa_len) == -1) 
                {
                    perror("getsockname() failed");
                }

                int port = (int) ntohs(sa.sin_port);

                printf("Local port is: %d\n", port);

                char * myIpString = replace_character(myIp, '.', ',');
              
                // convert port to FTP pasv connection format
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


                // TODO:  add timeout feature, that blocks for a finite period.

                // start listening...
                if (listen(datasockfd, BACKLOG) == -1) 
                {
                    perror("listen");
                    exit(1);
                    // TODO error??
                }

                isDataConnected = true; 
            }
            else
            {
                strcpy(response, "530 Not logged in.\n");
            }
        }
        else if ((strncmp(command, "NLST", 4) == 0)) 
        {
            if (isLoggedIn) 
            {       
                if (isDataConnected) 
                {
                    // used for accept() call
                    socklen_t data_sin_size;
                    struct sockaddr_storage data_their_addr;

                    data_sin_size = sizeof data_their_addr;
                    datatcpfd = accept( datasockfd, (struct sockaddr *)&data_their_addr, &data_sin_size);
                    
                    if (datatcpfd == -1) 
                    {
                        perror("accept");
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
                        perror("listFiles");
                        strcpy(response, "451 Requested action aborted: local error in processing.\n");
                    }    
                    else if (filesListed == -2) 
                    {
                        strcpy(response, "426 Connection closed; transfer aborted.\n");
                    }

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
                strcpy(response, "530 Not logged in.\n");
            }
        } 
        else 
        {
            strcpy(response, "502 Command not implemented..\n");
        }

        if (send(tcpfd, response, MAXDATASIZE, 0) == -1) 
        {
            perror("send");
            // TODO:    What to send client???
        }

        printf("CSftp: sent    :  %s",response);
    }
}


// the listening state
// spawn 4 threads and keeps them busy listening and accepting ftp clients

void * listening(int socket_fd) 
{              
    if (listen( socket_fd, BACKLOG) == -1) 
    {
        perror("listen");
        exit(1);
    }
    printf("CSftp: in listen state.\n");

    // infinite loop accepting connections.
    // Will interact with 4 clients simultaneously, each handled by a seperate thread.
    pthread_t thread0;
    pthread_t thread1;
    pthread_t thread2;
    pthread_t thread3;

    // While there is a free thread, execute messageState(socket_fd)
    while (1)
    {    
    pthread_create(&thread0, NULL, messageState, &socket_fd);
    pthread_create(&thread1, NULL, messageState, &socket_fd);
    pthread_create(&thread2, NULL, messageState, &socket_fd);
    pthread_create(&thread3, NULL, messageState, &socket_fd);

    pthread_join(thread0, NULL);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    }   

    return;
}


// FTP server by Sean Blair
int main(int argc, char **argv) 
{
    // Some of this code is based on http://beej.us/guide/bgnet/examples/server.c
    
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
    listening(sockfd);

    return 0;

}
