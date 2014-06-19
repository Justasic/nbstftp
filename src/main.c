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
#include "config.h"
#include "LICENSE.h"

int running = 1;
short port = 69;

// Fork to background unless otherwise specified
int nofork = 0;
int ipv4_only = 0;
int ipv6_only = 0;
char *configfile = NULL;

// Process the incoming packet.
void ProcessPacket(client_t client, void *buffer, size_t len)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
		Error(client, ERROR_ILLEGAL, "Invalid packet size");
		return;
	}

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
			printf("Got Acknowledgement packet for block %d\n", ntohs(p->blockno));
			break;
		case PACKET_WRQ:
			printf("Got write request packet\n");
                        Error(client, ERROR_UNDEFINED, "Operation not supported.");
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
			// mode can be "netascii", "octet", or "mail" case insensitive.
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

void PrintHelp(void)
{
	fprintf(stderr, "Usage: nbstftp [OPTION]... [INTERFACE [PORT]]\n");
	fprintf(stderr, "The no bullshit TFTP server, used to serve files over TFTP\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "-f, --nofork        Do not daemonize (don't fork to background)\n");
	fprintf(stderr, "-c, --config        Specify a config file\n");
	fprintf(stderr, "-4, --ipv4          Only bind to IPv4 interfaces\n");
	fprintf(stderr, "-6, --ipv6          Only bind to IPv6 interfaces\n");
	fprintf(stderr, "-p, --port          Specify a port to listen on\n");
	fprintf(stderr, "-h, --help          This message\n");
	fprintf(stderr, "-l, --license       Print license message\n");
	
	exit(EXIT_FAILURE);
}

void PrintLicense(void)
{
	// Print the license header.
	char *str = malloc(LICENSE_len+1);
	memcpy(str, LICENSE, LICENSE_len);
	str[LICENSE_len+1] = 0;
	fprintf(stderr, "%s", str);
	free(str);
	exit(EXIT_FAILURE);
}

void HandleArguments(int argc, char **argv)
{
	int wait_for_conf = 0, wait_for_port = 0;
	for (int i = 0; i < argc; ++i)
	{
		char *arg = argv[i];
		
		// Handle -- arguments.
		if (arg[0] == '-' && arg[1] == '-')
		{
			arg += 2;
			
			if (!strcasecmp(arg, "help"))
				PrintHelp();
			else if (!strcasecmp(arg, "license"))
				PrintLicense();
			else if (!strcasecmp(arg, "nofork"))
				nofork = 1;
			else if (!strcasecmp(arg, "config"))
			{ // Make sure they actually specified a config file...
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a config file!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_conf = 1;
			}
			else if (!strcasecmp(arg, "port"))
			{
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a port!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_port = 1;
			}
			else if (!strcasecmp(arg, "ipv4"))
				ipv4_only = 1;
			else if (!strcasecmp(arg, "ipv6"))
				ipv6_only = 1;
			else
			{
				fprintf(stderr, "Unknown argument: \"%s\"\n", arg);
				PrintHelp();
			}
		}
		// Handle single dash arguments.
		else if (arg[0] == '-' && arg[1] != '-')
		{
			arg++;
			
			if (!strcasecmp(arg, "h"))
				PrintHelp();
			else if (!strcasecmp(arg, "l"))
				PrintLicense();
			else if (!strcasecmp(arg, "f"))
				nofork = 1;
			else if (!strcasecmp(arg, "c"))
			{
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a config file!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_conf = 1;
			}
			else if (!strcasecmp(arg, "p"))
			{
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a port!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_port = 1;
			}
			else if (!strcasecmp(arg, "4"))
				ipv4_only = 1;
			else if (!strcasecmp(arg, "6"))
				ipv6_only = 1;
			else
			{
				fprintf(stderr, "Unknown argument: \"%s\"\n", arg);
				PrintHelp();
			}
		}
		else // Handle other arguments.
		{
			// Copy the config file
			if (wait_for_conf)
				configfile = arg;
			else if (wait_for_port)
				port = atoi(arg);
		}
	}
}

int main(int argc, char **argv)
{
	
	HandleArguments(argc, argv);
	
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
