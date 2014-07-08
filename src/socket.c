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

#include <sys/types.h>
#include <sys/socket.h>

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
	if (!addr)
		addr = inet_ntoa(saddr.in.sin_addr);
	
	socket_t *sock = nmalloc(sizeof(socket_t));
	sock->bindaddr = strdup(addr);
	sock->type = saddr.sa.sa_family;
	sock->fd = fd;
	sock->flags = 0;
	memcpy(&sock->addr, &saddr, sizeof(socketstructs_t));

    // Add it to the multiplexer
	if (binding)
	{
        printf("Adding to multiplexer!\n");
		if (AddToMultiplexer(sock) == -1)
		{
			close(fd);
            free(sock);
			return NULL;
		}
	}
	
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
	RemoveFromMultiplexer(s);

	// Now remove it from our vector
	vec_remove(&socketpool, s);
	// Close the socket
	if (closefd)
		close(s->fd);
	// Free a string then free itself.
	free(s->bindaddr);
	free(s);
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
	
	// Shutdown our multiplexer
	ShutdownMultiplexer();
}
