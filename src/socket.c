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
#include "socket.h"
#include "client.h"
#include "packets.h"
#include "misc.h"
#include "filesystem.h"
#include "config.h"
#include "process.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "vec.h"
#include "multiplexer.h"

socket_vec_t socketpool;
extern int port;

// This function will create and bind to a port and address. It will create and add
// the socket to the epoll loop. This should be called for each socket created by
// the config.
int BindToSocket(const char *addr, short port)
{
	assert(addr);

	int fd = 0;
	socketstructs_t saddr;
	memset(&saddr, 0, sizeof(socketstructs_t));

	// Get the address fanily
	saddr.sa.sa_family = strstr(addr, ":") != NULL ? AF_INET6 : AF_INET;

	printf("Address family is: %s\n", saddr.sa.sa_family == AF_INET6 ? "AF_INET6" : "AF_INET" );

	// Set our port
	*(saddr.sa.sa_family == AF_INET ? &saddr.in.sin_port : &saddr.in6.sin6_port) = htons(port);

	switch (inet_pton(saddr.sa.sa_family, addr, (saddr.sa.sa_family == AF_INET ? &saddr.in.sin_addr : &saddr.in6.sin6_addr)))
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
	if (bind(fd, &saddr.sa, saddr.sa.sa_family == AF_INET ? sizeof(saddr.in) : sizeof(saddr.in6)) < 0)
	{
		perror("bind failed");
		close(fd);
		return -1;
	}

	if (AddSocket(fd, addr, saddr.sa.sa_family, saddr, 1, NULL) == -1)
	{
		fprintf(stderr, "Failed to bind to socket!\n");
		return -1;
	}

	// Return success
	return 0;
}

int AddSocket(int fd, const char *addr, int type, socketstructs_t saddr, uint8_t binding, socket_t *s)
{
	if (!addr)
		addr = GetAddress(saddr);

	socket_t sock;
	sock.bindaddr = strdup(addr);
	sock.type = saddr.sa.sa_family;
	sock.fd = fd;
	sock.flags = 0;
	memcpy(&(sock.addr), &saddr, sizeof(socketstructs_t));

	// Add it to the multiplexer
	if (binding)
	{
		if (AddToMultiplexer(&sock) == -1)
		{
			close(fd);
			return -1;
		}
	}

	vec_push(&socketpool, sock);
	// Make sure we could add the socket.
	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to add socket to socket pool!\n");
		DestroySocket(sock, 0);
		return -1;
	}

	if (s)
		*s = sock;

	return 0;
}

int FindSocket(int fd, socket_t *s)
{
	socket_t sock;
	int i;

	vec_foreach(&socketpool, sock, i)
	{
		if (sock.fd == fd)
		{
			*s = sock;
			return 0;
		}
	}

	return -1;
}

short GetPort(socket_t s)
{
	// if IPv4, get IPv4 port
	if (s.addr.sa.sa_family == AF_INET)
		return ntohs(s.addr.in.sin_port);
	// Otherwise, get IPv6 port
	else if(s.addr.sa.sa_family == AF_INET6)
		return ntohs(s.addr.in6.sin6_port);

	return -1;
}

const char *GetAddress(socketstructs_t saddr)
{
	static char str[INET6_ADDRSTRLEN+1];

	return inet_ntop(saddr.sa.sa_family, &saddr.sa, str, INET6_ADDRSTRLEN);
}

void DestroySocket(socket_t s, uint8_t closefd)
{
	// First, remove it from the multiplexer system, but ONLY if we plan on closing it too.
	if (closefd)
		RemoveFromMultiplexer(s);

	// Now remove it from our vector
	for (int idx = 0; idx < socketpool.length; idx++)
	{
		if (socketpool.data[idx].fd == s.fd && !strcmp(socketpool.data[idx].bindaddr, s.bindaddr))
		{
			vec_splice(&socketpool, idx, 1);
			break;
		}
	}

	// Close the socket
	if (closefd)
		close(s.fd);
	// Free a string then free itself.
	free(s.bindaddr);
}

// Initialize the Epoll socket descriptor as well as all the
// sockets we should bind to.
int InitializeSockets(void)
{
	assert(config);

	if (InitializeMultiplexer() == -1)
		return -1;

	vec_init(&socketpool);
	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to allocate socket pool!\n");
		return -1;
	}

	listen_t *block;
	int i = 0, bound = 0;
	vec_foreach(&config->listenblocks, block, i)
	{
		// Our defaults if they didn't specify them.
		if (block->port == -1)
			block->port = 69;

		if (!block->bindaddr)
			block->bindaddr = strdup("::");

		if (BindToSocket(block->bindaddr, block->port) == -1)
		{
			int isipv6 = strstr(block->bindaddr, ":") != NULL;
			fprintf(stderr, "Failed to bind to %c%s%c:%d\n",
				(isipv6 ? '[' : '\0'), block->bindaddr, (isipv6 ? ']' : '\0'), block->port);
			continue;
		}
		bound = 1;
	}

	// If we didn't find an interface to bind to, exit.
	return bound ? 0 : -1;
}

void ShutdownSockets(void)
{
	socket_t s;
	int i;

	// Close all the sockets
	vec_foreach(&socketpool, s, i)
	{
		close(s.fd);
		free(s.bindaddr);
	}

	vec_deinit(&socketpool);

	// Shutdown our multiplexer
	ShutdownMultiplexer();
}

// Queue packets for sending -- internal function
void QueuePacket(client_t *c, packet_t *p, size_t len, uint8_t allocated)
{
	// We're adding another packet.
	// Try and keep up with the required queue
	if (c->packetqueue_vec.length+1 >= c->packetqueue_vec.capacity)
		vec_reserve(&c->packetqueue_vec, c->packetqueue_vec.length * 2);

	packetqueue_t pack;
	pack.p = p;
	pack.len = len;
	pack.allocated = allocated;

	vec_push(&c->packetqueue_vec, pack);

	// We're a resend, no need to redo the packet copy.
	if (allocated != 2)
	{
		// Copy the packet structure
		memcpy(&c->lastpacket, &pack, sizeof(packetqueue_t));
		c->lastpacket.p = nmalloc(len);
		memcpy(c->lastpacket.p, p, len);
	}
	
	// Mark the client as waiting again
	c->waiting = 1;
	c->nextresend = time(NULL) + 5;

	// We're ready to write.
	SetSocketStatus(&c->s, SF_WRITABLE | SF_READABLE);
}

// Send packets out the socket, this will be called by the multiplexers
// system in one of the multiplexers files
int SendPackets(socket_t s)
{
	packetqueue_t pq;
	client_t *c = NULL;
	int cidx, idx;

	// Iterate over the entire client pool because there may be more than
	// one client which needs to send packets. This should probably be
	// improved in the future but it works for now.
	vec_foreach(&clientpool, c, cidx)
	{
		vec_foreach(&c->packetqueue_vec, pq, idx)
		{
			printf("Sending packet %d length %zu\n", ntohs(pq.p->opcode), pq.len);

			int sendlen = sendto(c->s.fd, pq.p, pq.len, 0, &c->s.addr.sa, c->s.addr.sa.sa_family == AF_INET ? sizeof(c->s.addr.in) : sizeof(c->s.addr.in6));
			if (sendlen == -1)
			{
				perror("sendto failed");
				return -1;
			}

			if (pq.allocated && pq.allocated != 2)
				free(pq.p);
		}

		vec_clear(&c->packetqueue_vec);

		SetSocketStatus(&c->s, SF_READABLE);

		if (c->destroy)
		{
			// If we're reading or writing a file, close it.
			if (c->f)
			{
				fflush(c->f);
				fclose(c->f);
			}
			
			RemoveClient(c);
		}
	}

	return 0;
}

int ReceivePackets(socket_t s)
{
	socketstructs_t ss;
	socklen_t addrlen = sizeof(ss);
	uint8_t buf[MAX_PACKET_SIZE];
	errno = 0;
	memset(buf, 0, sizeof(buf));
	size_t recvlen = recvfrom(s.fd, buf, MAX_PACKET_SIZE, 0, &ss.sa, &addrlen);

	// The kernel either told us that we need to read again
	// or we received a signal and are continuing from where
	// we left off.
	if (recvlen == -1 && (errno == EAGAIN || errno == EINTR))
		return 0;
	else if (recvlen == -1)
	{
		fprintf(stderr, "Socket: Received an error when reading from the socket: %s\n", strerror(errno));
		return -1;
	}

	// Create our temp client socket
	socket_t cs;
	cs.fd = s.fd;
	cs.type = s.type;
	cs.addr = ss;

	// Either find the client or allocate a new client and socket
	client_t *c = FindOrAllocateClient(cs);

	printf("Received %zu bytes from %s:%d on socket %d\n", recvlen, GetAddress(cs.addr), GetPort(cs), s.fd);

	// Process the packet received.
	ProcessPacket(c, buf, recvlen);

	return 0;
}
