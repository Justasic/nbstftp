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
#define _GNU_SOURCE 1
#include "socket.h"
#include "client.h"
#include "packets.h"
#include "misc.h"
#include "filesystem.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <alloca.h>
#include <assert.h>
#include <errno.h>
// #include <ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "vec.h"

typedef struct epoll_event epoll_t;
socket_vec_t socketpool;
int EpollHandle = -1;
extern int port;

epoll_t *events;
size_t events_len = 5;

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
			if ((len + sizeof(packet_t) > MAX_PACKET_SIZE))
			{
				fprintf(stderr, "Received an oversided data packet! Terminating transfer.\n");
				Error(c, ERROR_ILLEGAL, "Sent an oversided packet.");
			}

			if ((len - sizeof(packet_t)) < 512)
				printf("Got end of data packet?\n");
			break;
		case PACKET_ERROR:
		{
			// icky! -- Cast the packet_t pointer to a uint8_t then increment 4 bytes, then cast
			// to a const char * and send to printf.
			const char *error = strndupa(((const char*)p) + sizeof(packet_t), 512);
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
			char *addr1 = inet_ntoa(c->s->addr.in.sin_addr);
			printf("Got Acknowledgement packet for block %d from %s\n", ntohs(p->blockno), addr1);
			
			if (c->sendingfile)
			{
				printf("Client is sending file\n");
				uint8_t buf[512];
				memset(buf, 0, sizeof(buf));
				size_t readlen = fread(buf, 1, sizeof(buf), c->f);
				
				c->currentblockno++;
				
				// Sending a file
				SendData(c, buf, readlen);
				
				// We're at the end of the file.
				if (MIN(512, readlen) != 512)
				{
					printf("Finished sending file\n");
					// Close the file and clean up.
					fclose(c->f);
					RemoveClient(c);
					break;
				}
			}

			break;
		}
		case PACKET_WRQ:
		{
			const char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);

			const char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);

			printf("Got write request packet for file \"%s\" in mode %s\n", filename, mode);
			Error(c, ERROR_UNDEFINED, "Operation not yet supported.");
			break;
		}
		case PACKET_RRQ:
		{
			// Get the filename and modes
			//
			// Since we dig only past the first value in the struct, we only
			// get the size of that first value (eg, the uint16_t)
			// Use strndupa to use the stack frame for temporary allocation with a
			// max length of 512 bytes. This will prevent buffer-overflow exploits (or so I hope)
			const char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);
			// This one is a bit weirder. We get the size of the uint16 like
			// we did above but also skip our filename string AND the remaining
			// null byte which strlen does not include.
			const char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
			
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

int BindToSocket(const char *addr, short port)
{
	assert(addr);

	int fd = 0;
	socketstructs_t saddr;
	memset(&saddr, 0, sizeof(socketstructs_t));

	saddr.sa.sa_family = strstr(addr, ":") != NULL ? AF_INET6 : AF_INET;
	saddr.in.sin_port = htons(port);
	
	switch (inet_pton(saddr.sa.sa_family, addr, &saddr.in.sin_addr))
	{
		case 1: // Success.
			break;
		case 0:
			fprintf(stderr, "Invalid %s bind address: %s\n",
				saddr.sa.sa_family == AF_INET ? "IPv4" : "IPv6", addr);
		default:
			perror("inet_pton");
			return -1;
	}

	// Create the UDP listening socket
	if ((fd = socket(saddr.sa.sa_family, SOCK_DGRAM, 0)) < 0)
	{
		perror("Cannot create socket\n");
		return -1;
	}

	// Set it as non-blocking
	int flags = fcntl(fd, F_GETFL, 0);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		perror("fcntl O_NONBLOCK");
		close(fd);
		return -1;
	}

	// Bind to the socket
	if (bind(fd, &saddr.sa, sizeof(saddr.sa)) < 0)
	{
		perror("bind failed");
		close(fd);
		return -1;
	}
	
#if 0
	// Test to make sure this shit really is nonblocking
	{
		socketstructs_t addr;
		socklen_t addrlen = sizeof(addr.sa);
		uint8_t buf[MAX_PACKET_SIZE];
		size_t recvlen = recvfrom(fd, buf, sizeof(buf), 0, &addr.sa, &addrlen);
		if (recvlen == -1)
		{
			if (errno == EAGAIN)
				printf("EAGAIN! We're nonblocking!\n");
			else
				fprintf(stderr, "What the fuck? %s\n", strerror(errno));
		}
	}
#endif

	if (!AddSocket(fd, addr, saddr.sa.sa_family, saddr, 1))
	{
		fprintf(stderr, "Failed to bind to socket!\n");
		return -1;
	}

	// Return success
	return 0;
}

socket_t *AddSocket(int fd, const char *addr, int type, socketstructs_t saddr, uint8_t binding)
{
	// Add it to EPoll
	if (binding)
	{
		epoll_t ev;
		memset(&ev, 0, sizeof(epoll_t));
		
		ev.events = EPOLLIN;
		ev.data.fd = fd;
		
		if (epoll_ctl(EpollHandle, EPOLL_CTL_ADD, fd, &ev) == -1)
		{
			fprintf(stderr, "Unable to add fd %d from epoll: %s\n", fd, strerror(errno));
			close(fd);
			return NULL;
		}
	}
	
	if (!addr)
		addr = inet_ntoa(saddr.in.sin_addr);
	
	socket_t *sock = nmalloc(sizeof(socket_t));
	sock->bindaddr = strdup(addr);
	sock->type = saddr.sa.sa_family;
	sock->fd = fd;
// 	sock->addr = saddr;
	sock->flags = EPOLLIN;
	memcpy(&sock->addr, &saddr, sizeof(socketstructs_t));
	
	vec_push(&socketpool, sock);
	// Make sure we could add the socket.
	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to add socket to socket pool!\n");
		DestroySocket(sock, 0);
		return NULL;
	}
	
	return sock;
}

socket_t *FindSocket(int fd)
{
	socket_t *s = NULL;
	int i;

	vec_foreach(&socketpool, s, i)
	{
		if (s->fd == fd)
			return s;
	}

	return NULL;
}

short GetPort(socket_t *s)
{
	// if IPv4, get IPv4 port
	if (s->addr.sa.sa_family == AF_INET)
		return ntohs(s->addr.in.sin_port);
	// Otherwise, get IPv6 port
	else if(s->addr.sa.sa_family == AF_INET6)
		return htons(s->addr.in6.sin6_port);
	
	return -1;
}

void DestroySocket(socket_t *s, uint8_t closefd)
{
	// First, remove it from the EPoll system.
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));
	ev.data.fd = s->fd;

	if (epoll_ctl(EpollHandle, EPOLL_CTL_DEL, ev.data.fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to remove fd %d from epoll: %s\n", s->fd, strerror(errno));
		vec_remove(&socketpool, s);
		free(s->bindaddr);
		free(s);
		return;
	}

	// Now remove it from our vector
	vec_remove(&socketpool, s);
	// Close the socket
	if (closefd)
		close(s->fd);
	// Free a string then free itself.
	free(s->bindaddr);
	free(s);
}

void SetSocketStatus(socket_t *s, int status)
{
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));

	ev.events = (status & SF_READABLE ? EPOLLIN : 0) | (status & SF_WRITABLE ? EPOLLOUT : 0);
	ev.data.fd = s->fd;
	s->flags = ev.events;

	if (epoll_ctl(EpollHandle, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to set fd %d from epoll: %s\n", s->fd, strerror(errno));
		vec_remove(&socketpool, s);
		free(s->bindaddr);
		free(s);
		return;
	}
}

// Initialize the Epoll socket descriptor as well as all the
// sockets we should bind to.
int InitializeSockets(void)
{
	assert(config);

	EpollHandle = epoll_create(4);

	events = reallocarray(NULL, events_len, sizeof(epoll_t));
	if (!events)
		return -1;

	vec_init(&socketpool);

	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to allocate socket pool!\n");
		return -1;
	}

// 	// TODO: Get list of sockets from config
// 	if (BindToSocket(config->bindaddr, port) == -1)
// 		return -1;

	return 0;
}

void ShutdownSockets(void)
{
	socket_t *s;
	int i;

	// Close all the sockets
	vec_foreach(&socketpool, s, i)
	{
		close(s->fd);
		free(s->bindaddr);
		free(s);
	}

	vec_deinit(&socketpool);

	// Close the EPoll handle.
	close(EpollHandle);

	// Free our events
	free(events);
}

void ProcessSockets(void)
{
	if (socketpool.length > events_len)
	{
		printf("Resizing array to support events_len\n");
		epoll_t *newptr = reallocarray(events, events_len * 2, sizeof(epoll_t));
		if (!newptr)
		{
			fprintf(stderr, "Failed to allocate more memory to watch events on more sockets! (%s)\n",
				strerror(errno));
		}
		else
		{
			events_len *= 2;
			events = newptr;
		}
	}
	printf("Entering epoll_wait\n");

	int total = epoll_wait(EpollHandle, events, events_len, config->readtimeout * 1000);

	if (total == -1)
	{
		if (errno != EINTR)
		{
			if (errno == EFAULT)
			{
				fprintf(stderr, "EFAULT\n");
				exit(1);
			}
			else
				fprintf(stderr, "Error processing sockets: %s\n", strerror(errno));
		}
		return;
	}
	
	if (total)
		printf("epoll_wait returned %d descriptors\n", total);

// 	epoll_t *next = events;
	for (int i = 0; i < total; ++i)
	{
		epoll_t *ev = &events[i];

		socket_t *s = FindSocket(ev->data.fd);
		if (!s)
		{
			printf("Could not find socket %d\n", ev->data.fd);
			continue;
		}

		if (ev->events & (EPOLLHUP | EPOLLERR))
		{
			printf("Epoll error reading socket %d, destroying.\n", s->fd);
			DestroySocket(s, 1);
			continue;
		}

		// process socket read events.
		if (ev->events & EPOLLIN)
		{
// 			printf("Received read on socket %d port %d\n", s->fd, GetPort(s));
			socketstructs_t ss;
			socklen_t addrlen = sizeof(ss);
			uint8_t buf[MAX_PACKET_SIZE];
			size_t recvlen = recvfrom(s->fd, buf, sizeof(buf), 0, &ss.sa, &addrlen);

			// The kernel either told us that we need to read again
			// or we received a signal and are continuing from where
			// we left off.
			if (recvlen == -1 && (errno == EAGAIN || errno == EINTR))
				continue;
			else if (recvlen == -1)
			{
				fprintf(stderr, "Socket: Received an error when reading from the socket: %s\n", strerror(errno));
// 				running = 0;
				DestroySocket(s, 1);
				continue;
			}
			
			// Create our temp client socket
			socket_t cs;
			cs.fd = s->fd;
			cs.type = s->type;
			cs.addr = ss;
			
			// Either find the client or allocate a new client and socket
			client_t *c = FindOrAllocateClient(&cs);

			printf("Received %zu bytes from %s on socket %d\n", recvlen, inet_ntoa(cs.addr.in.sin_addr), s->fd);

			// Process the packet received.
			ProcessPacket(c, buf, recvlen);
		}

		// Process socket write events
		if (ev->events & EPOLLOUT && SendPackets() == -1)
		{
			printf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}
	}
}
