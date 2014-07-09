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
#include "sysconf.h"

#ifndef HAVE_SELECT
# error You probably shouldn't be trying to compile a select multiplexer on a select-unsupported platform. Try again.
#endif

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#ifndef _WIN32
# include <unistd.h>
#endif

// #include "client.h"
#include "multiplexer.h"
#include "socket.h"
#include "config.h"
#include "vec.h"
#include "process.h"

static fd_set readfds, writefds;
static int maxfd = 0;

int AddToMultiplexer(socket_t *s)
{
	FD_SET(s->fd, &readfds);
	maxfd = s->fd;
	return 0;
}

int RemoveFromMultiplexer(socket_t *s)
{
	FD_CLR(s->fd, &readfds);
	FD_CLR(s->fd, &writefds);
	maxfd--;
	return 0;
}

int SetSocketStatus(socket_t *s, int status)
{
	assert(s);
	
	if (status & SF_READABLE && !(s->flags & SF_READABLE))
	{
		FD_SET(s->fd, &readfds);
// 		event = GetChangeEvent();
// 		EV_SET(event, s->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	}
	
	if (!(status & SF_READABLE) && s->flags & SF_READABLE)
	{
		FD_CLR(s->fd, &writefds);
// 		event = GetChangeEvent();
// 		EV_SET(event, s->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	}
	
	if (status & SF_WRITABLE && !(s->flags & SF_WRITABLE))
	{
		FD_SET(s->fd, &writefds);
// 		event = GetChangeEvent();
// 		EV_SET(event, s->fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	}
	
	if (!(status & SF_WRITABLE) && s->flags & SF_WRITABLE)
	{
		FD_CLR(s->fd, &writefds);
// 		event = GetChangeEvent();
// 		EV_SET(event, s->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	}
	
	s->flags = status;
	
	return 0;
}

int InitializeMultiplexer(void)
{
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);

	return 0;
}

int ShutdownMultiplexer(void)
{
	// nothing, stack unwinds and clears what we allocated.
	return 0;
}

extern socket_vec_t socketpool;
void ProcessSockets(void)
{
	fd_set read = readfds, write = writefds, error = readfds;
	// Default read time
	struct timeval seltv = { config->readtimeout, 0 };
	
	printf("Entering select\n");
	
	int ret = select(maxfd + 1, &read, &write, &error, &seltv);
	
	if (ret == -1)
		fprintf(stderr, "Failed to select(): %s\n", strerror(errno));
	else if (ret)
	{
		int processed = 0, idx = 0;
		socket_t *s;
		vec_foreach(&socketpool, s, idx)
		{
			int has_read = FD_ISSET(s->fd, &read);
			int has_write = FD_ISSET(s->fd, &write);
			int has_error = FD_ISSET(s->fd, &error);
			
			if (has_error)
			{
				printf("select() error reading socket %d, destroying.\n", s->fd);
				DestroySocket(s, 1);
				continue;
			}
			
			if (has_read)
			{
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
// 					running = 0;
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
			
			if (has_write && SendPackets() == -1)
			{
				printf("Destorying socket due to send failure!\n");
				DestroySocket(s, 1);
			}
		}
	}
}