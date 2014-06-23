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
#define _POSIX_C_SOURCE 1
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
#include <signal.h>
#include <assert.h>

#include "packets.h"
#include "config.h"
#include "commandline.h"
#include "filesystem.h"
#include "client.h"
#include "misc.h"
#include "signalhandler.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

int running = 1;
short port = -1;

// Fork to background unless otherwise specified
int nofork = -1;
// int ipv4_only = 0;
// int ipv6_only = 0;
char *configfile = NULL;

void WritePID(void)
{
	FILE *f = NULL;
	// Make sure the process doesn't already exist.
	if (FileExists(config->pidfile))
	{
		f = fopen(config->pidfile, "r");
		if (!f)
		{
			// wtf? the pid file exists but it doesnt?
			// whatever. just write our own pid file.
			goto write;
		}
		
		// Read the PID file, 
		char pidstr[1024];
		memset(pidstr, 0, sizeof(pidstr));
		fread(pidstr, 1, sizeof(pidstr), f);
		fclose(f);
		
		// Convert from string to int.
		pid_t pid = atoi(pidstr);
		
		// Check if the process is running.
		if (kill(pid, 0) == 0)
			die("A nbstftp daemon is already running!");
		else
		{
			printf("Removing stale pid file %s\n", config->pidfile);
			unlink(config->pidfile);
		}
	}
write:
	f = fopen(config->pidfile, "w");
	if (!f)
	{
		fprintf(stderr, "WARNING: Failed to open PID file for write (File: %s)\n", config->pidfile);
		return;
	}
	
	fprintf(f, "%d", getpid());
	fclose(f);
}

// Process the incoming packet.
void ProcessPacket(client_t *c, void *buffer, size_t len)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
// 		Error(c, ERROR_ILLEGAL, "Invalid packet size");
		printf("Received an invalidly sized packet.\n");
		return;
	}
	
	assert(buffer);

	const packet_t *p = buffer;
	
	switch(ntohs(p->opcode))
	{
		case PACKET_DATA:
			printf("Got a data packet\n");
			if ((len - sizeof(packet_t)) < 512)
				printf("Got end of data packet?\n");
			break;
		case PACKET_ERROR:
		{
			// icky! -- Cast the packet_t pointer to a uint8_t then increment 4 bytes, then cast
			// to a const char * and send to printf.
			// WARNING: This could buffer-overflow printf, need to use strlcpy or something safer.
			const char *error = ((const char*)p) + sizeof(packet_t);
			printf("Error: %s (%d)\n", error, ntohs(p->blockno));
			
			// Send an Acknowledgement packet.
			Acknowledge(c, 1);
			
			// For whatever reason, BIOSes send a WRQ packet to verify the
			// file exists and then cancels immediately upon receiving the
			// first packet. This makes the server freak the fuck out and
			// do things wrong. So here we deallocate if we run into any
			// kind of error.
			
                        if (c->sendingfile)
                        {
				printf("Aborting file transfer.\n");
				// Close the file and clean up.
				fclose(c->f);
				RemoveClient(c);
			}
			
			break;
		}
		case PACKET_ACK:
		{
			char *addr1 = inet_ntoa(c->addr.in.sin_addr);
			printf("Got Acknowledgement packet for block %d from %s\n", ntohs(p->blockno), addr1);
			
			if (c->sendingfile)
			{
				printf("Client is sending file\n");
				uint8_t buf[512];
				memset(buf, 0, sizeof(buf));
				size_t readlen = fread(buf, 1, sizeof(buf), c->f);
				
				c->currentblockno++;
				// We're at the end of the file.
				if (MIN(512, readlen) != 512)
				{
					SendData(c, buf, readlen);
					printf("Finished sending file\n");
					// Close the file and clean up.
					fclose(c->f);
					RemoveClient(c);
					break;
				}
				SendData(c, buf, readlen);
			}

			break;
		}
		case PACKET_WRQ:
			printf("Got write request packet\n");
			Error(c, ERROR_UNDEFINED, "Operation not yet supported.");
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

			// We don't support mail-mode
			if (!strcasecmp(mode, "mail"))
			{
				Error(c, ERROR_ILLEGAL, "Mail mode not supported by NBSTFTP");
				break;
			}
			
			int imode = strcasecmp(mode, "netascii");

			char tmp[(1 << 16)];
			sprintf(tmp, "%s/%s", config->directory, filename);

			if (!FileExists(tmp))
				Error(c, ERROR_NOFILE, "File %s does not exist on the filesystem.", tmp);

			FILE *f = fopen(tmp, (imode == 0 ? "rt" : "rb"));
			if (!f)
			{
				fprintf(stderr, "Failed to open file %s for sending: %s\n", tmp, strerror(errno));
				Error(c, ERROR_NOFILE, "Cannot open file: %s", strerror(errno));
				break;
			}
			
			printf("File \"%s\" is available, sending first packet\n", tmp);

			// file buffer
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));
			size_t readlen = fread(buf, 1, sizeof(buf), f);
			c->f = f;
			c->currentblockno = 1;
			c->sendingfile = 1;
			SendData(c, buf, readlen);
			
			break;
                }
		default:
			printf("Got unknown packet: %d\n", ntohs(p->opcode));
			// Unknown packets are ignored according to the RFC.
			//Error(c, ERROR_ILLEGAL, "Unknown packet");
			break;
	}
}

int main(int argc, char **argv)
{
	HandleArguments(argc, argv);
	
	RegisterSignalHandlers();

	if (!configfile)
		configfile = "nbstftp.conf";

	if (ParseConfig(configfile) != 0)
		die("FATAL: Failed to parse the config file!");


	socketstructs_t myaddr;
	int recvlen, fd;
	unsigned char buf[MAX_PACKET_SIZE];
	
	// Write the PID file -- Also check for any other
	// running versions of us.
	WritePID();

	// Create the UDP listening socket
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("Cannot create socket\n");
		return EXIT_FAILURE;
	}
	
	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.in.sin_family = AF_INET;
	
	switch (inet_pton(AF_INET, config->bindaddr, &myaddr.in.sin_addr))
	{
		case 1: // Success.
			break;
		case 0:
			die("Invalid ipv4 bind address: %s", config->bindaddr);
		default:
			perror("inet_pton");
			return EXIT_FAILURE;
	}


// 	myaddr.in.sin_addr.s_addr = htonl(INADDR_ANY);
	// if the user didn't specify a port on the command line already
	// use our own from the config or use the default (also from the
	// config)
	if (port == -1)
		port = config->port;
	myaddr.in.sin_port = htons(port);

	if (bind(fd, &myaddr.sa, sizeof(myaddr.sa)) < 0)
	{
		perror("bind failed");
		goto cleanup;
	}
	
	// Change the user and group id.
	if (SwitchUserAndGroup(config->user, config->group) == 1)
	{
		fprintf(stderr, "Failed to set user id or group id!\n");
		goto cleanup;
	}
	
	if (nofork == -1)
		nofork = !config->daemonize;
	// Go away.
	Daemonize();
	
	// Enter idle loop.
	while (running)
	{
		printf("Waiting on port %d\n", port);
		
                socketstructs_t addr;
		socklen_t addrlen = sizeof(socketstructs_t);
		recvlen = recvfrom(fd, buf, sizeof(buf), 0, &addr.sa, &addrlen);

		// The kernel either told us that we need to read again
		// or we received a signal and are continuing from where
		// we left off.
		if (recvlen == -1 && (errno == EAGAIN || errno == EINTR))
			continue;
		else if (recvlen == -1)
		{
			fprintf(stderr, "FATAL: Received an error when reading from the socket: %s\n", strerror(errno));
			running = 0;
			continue;
		}

                client_t *c = FindOrAllocateClient(addr, fd);
		
		printf("Received %d bytes from %s\n", recvlen, inet_ntoa(c->addr.in.sin_addr));
		
		// Process the packet received.
		ProcessPacket(c, buf, recvlen);
	}

cleanup:
	
	// Close the file descriptor.
	close(fd);
	
	// Remove our PID
	if (config)
		unlink(config->pidfile);

	// Cleanup memory
	DeallocateConfig(config);
	
	return EXIT_SUCCESS;
}
