#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include "packets.h"
#include "LICENSE.h"

int running = 1;
int port = 69;

typedef union socketstructs_s
{
    struct sockaddr_in in;
    struct sockaddr_in in6;
    struct sockaddr sa;
} socketstructs_t;

int main(int argc, char **argv)
{
    socketstructs_t myaddr, remoteaddr;
    socklen_t addrlen = sizeof(remoteaddr.in);
    int recvlen, fd;

    unsigned char buf[MAX_PACKET_SIZE];

    // Create the UDP listening socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Cannot create socket\n");
        return EXIT_FAILURE;
    }

    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.in.sin_family = AF_INET;
    myaddr.in.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.in.sin_port = htons(port);

    if (bind(fd, &myaddr.sa, sizeof(myaddr.sa)) < 0)
    {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    while(running)
    {
        printf("Waiting on port %d\n", port);
        recvlen = recvfrom(fd, buf, sizeof(buf), 0, &remoteaddr.sa, &addrlen);
        printf("Received %d bytes\n", recvlen);
    }
    return EXIT_SUCCESS;
}
