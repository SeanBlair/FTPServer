#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>

#include "dir.h"
#include "usage.h"


///////  from demo server  ////////
// #include <stdlib.h>
// #include <unistd.h>
// #include <errno.h>
#include <string.h>
// #include <netinet/in.h>
#include <netdb.h>
// #include <arpa/inet.h>
// #include <sys/wait.h>
// #include <signal.h>


// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.

int main(int argc, char **argv) {

    
    struct addrinfo hints, *servinfo;  // not sure what these for... look in tutorial.


    // This is some sample code feel free to delete it
    // This is the main program for the thread version of nc

    int rv;   // return value?
    
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




    // This is how to call the function in dir.c to get a listing of a directory.
    // It requires a file descriptor, so in your code you would pass in the file descriptor 
    // returned for the ftp server's data connection
    
    printf("Printed %d directory entries\n", listFiles(1, "."));
    return 0;

}
