/*
 * Copyright (c) 2014, Justin Crawford <Justasic@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <bsd/string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "packets.h"
#include "LICENSE.h"

int running = 1;
short port = 69;

// Process the incoming packet.
void ProcessPacket(client_t client, void *buffer, size_t len)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
		Error(client, ERROR_ILLEGAL, "Invalid packet size");
		return;
	}

        printf("Received a packet with %d len\n", len);
	
	packet_t *p = malloc(len);
        memset(p, 0, len);
	memcpy(p, buffer, len);
	
	switch(ntohs(p->opcode))
	{
		case PACKET_DATA:
			printf("Got a data packet\n");
			if (len < 512)
				printf("Got end of data packet?\n");
			break;
		case PACKET_ERROR:
		{
			// icky! -- Cast the packet_t pointer to a uint8_t then increment 4 bytes, then cast
			// to a const char * and send to printf.
			// WARNING: This could buffer-overflow printf, need to use strlcpy or something safer.
			const char *error = ((const char*)p) + sizeof(packet_t);
			printf("Error: %s (%d)\n", error, ntohs(p->blockno));
			break;
		}
		case PACKET_ACK:
			printf("Got Acknowledgement packet\n");
			break;
		case PACKET_WRQ:
			printf("Got write request packet\n");
			break;
		case PACKET_RRQ:
                {
                        // Get the filename and modes
                        // WARNING: This needs to be fixed with proper length checking!
                        //
                        // Since we dig only past the first value in the struct, we only
                        // get the size of that first value (eg, the uint16_t)
                        const char *filename = ((const char *)p) + sizeof(uint16_t);
                        // This one is a bit weirder. We get the size of the uint16 like
                        // we did above but also skip our filename string AND the remaining
                        // null byte which strlen does not include.
                        const char *mode = ((const char *)p) + (sizeof(uint16_t) + strlen(filename) + 1);
			printf("Got read request packet: \"%s\" -> \"%s\"\n", filename, mode);
			break;
                }
		default:
			printf("Got unknown packet: %d\n", ntohs(p->opcode));
                        // Unknown packets are ignored according to the RFC.
			//Error(client, ERROR_ILLEGAL, "Unknown packet");
			break;
	}
	
	free(p);
}

int main(int argc, char **argv)
{
	socketstructs_t myaddr;

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
	
	// Enter idle loop.
	while(running)
	{
		printf("Waiting on port %d\n", port);
		client_t c;
		// Clear our client struct
		memset(&c, 0, sizeof(client_t));
		socklen_t addrlen = sizeof(c.addr);
		
		recvlen = recvfrom(fd, buf, sizeof(buf), 0, &c.addr.sa, &addrlen);
 		c.fd = fd;
		
		printf("Received %d bytes from %s\n", recvlen, inet_ntoa(c.addr.in.sin_addr));
		
		// Process the packet received.
		ProcessPacket(c, buf, recvlen);
		
		// Send an error packet for the timebeing.
		Error(c, ERROR_NOFILE, "Nothing is implemented! But it works! :D");
	}
	
	// Close the file descriptor.
	close(fd);
	
	return EXIT_SUCCESS;
}
