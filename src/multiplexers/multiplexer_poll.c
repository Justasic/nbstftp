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

#ifndef HAVE_POLL
# error You probably shouldn't be trying to compile an poll multiplexer on a poll-unsupported platform. Try again.
#endif

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "socket.h"
#include "multiplexer.h"
#include "misc.h"
#include "vec.h"
#include "config.h"
#include "client.h"
#include "process.h"

#ifndef _WIN32
# include <unistd.h>
#endif

typedef struct pollfd poll_t;
static vec_t(poll_t) events;

int AddToMultiplexer(socket_t *s)
{
	poll_t ev;
	memset(&ev, 0, sizeof(poll_t));
	
	ev.fd = s->fd;
	ev.events = POLLIN;
	
	s->flags = SF_READABLE;
	
	vec_push(&events, ev);
	return 0;
}

int RemoveFromMultiplexer(socket_t *s)
{
	// Find our socket
	for (int idx = 0; idx < events.length; idx++)
	{
		if (events.data[idx].fd == s->fd)
		{
			vec_splice(&events, idx, 1);
			return 0;
		}
	}

	return -1;
}

int SetSocketStatus(socket_t *s, int status)
{
	// Find our socket, then set the flags needed.
	for (int idx = 0; idx < events.length; idx++)
	{
		if (events.data[idx].fd == s->fd)
		{
			poll_t *ev = &events.data[idx];
			ev->events = (status & SF_READABLE ? POLLIN : 0) | (status & SF_WRITABLE ? POLLOUT : 0);
			s->flags = status;
			return 0;
		}
	}
	
	return -1;
}

int InitializeMultiplexer(void)
{
	vec_init(&events);
	
	if (errno == ENOMEM)
		return -1;
	
	return 0;
}

int ShutdownMultiplexer(void)
{
	vec_deinit(&events);
	return 0;
}

void ProcessSockets(void)
{
	printf("Entering poll\n");
	
	int total = poll(&vec_first(&events), events.length, config->readtimeout * 1000);
	
	if (total < 0)
	{
		if (errno != EINTR)
			fprintf(stderr, "Error processing sockets: %s\n", strerror(errno));
		return;
	}
	
	for (int i = 0, processed = 0; i < events.length && processed != total; ++i)
	{
		poll_t *ev = &events.data[i];
		
		if (ev->revents != 0)
			processed++;
		else // Nothing to do, move on.
			continue;
		
		socket_t *s = FindSocket(ev->fd);
		if (!s)
		{
			fprintf(stderr, "Unknown socket %d in poll() multiplexer.\n", ev->fd);
			continue;
		}
		
		if (ev->revents & (POLLERR | POLLRDHUP))
		{
			printf("Epoll error reading socket %d, destroying.\n", s->fd);
			DestroySocket(s, 1);
			continue;
		}
		
		if (ev->revents & POLLIN)
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
		
		if (ev->revents & POLLOUT && SendPackets() == -1)
		{
			printf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}
	}
}