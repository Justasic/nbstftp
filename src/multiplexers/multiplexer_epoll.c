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

#ifndef HAVE_SYS_EPOLL_H
# error You probably shouldn't be trying to compile an epoll multiplexer on a epoll-unsupported platform. Try again.
#endif

#include "socket.h"
#include "misc.h"
#include "config.h"
#include "client.h"
#include "process.h"

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct epoll_event epoll_t;
// Static because it never leaves this file.
static int EpollHandle = -1;
static epoll_t *events;
static size_t events_len = 5;

int AddToMultiplexer(socket_t *s)
{
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));
	
	ev.events = EPOLLIN;
	ev.data.fd = s->fd;
	s->flags = SF_READABLE;
	
	if (epoll_ctl(EpollHandle, EPOLL_CTL_ADD, s->fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to add fd %d from epoll: %s\n", s->fd, strerror(errno));
		return -1;
	}
	
	return 0;
}

int RemoveFromMultiplexer(socket_t *s)
{
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));
	ev.data.fd = s->fd;
	
	if (epoll_ctl(EpollHandle, EPOLL_CTL_DEL, ev.data.fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to remove fd %d from epoll: %s\n", s->fd, strerror(errno));
		return -1;
	}
	
	return 0;
}

int SetSocketStatus(socket_t *s, int status)
{
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));
	
	ev.events = (status & SF_READABLE ? EPOLLIN : 0) | (status & SF_WRITABLE ? EPOLLOUT : 0);
	ev.data.fd = s->fd;
	s->flags = status;
	
	if (epoll_ctl(EpollHandle, EPOLL_CTL_MOD, ev.data.fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to set fd %d from epoll: %s\n", s->fd, strerror(errno));
		return -1;
	}
	return 0;
}

int InitializeMultiplexer(void)
{
	EpollHandle = epoll_create(4);
	
	events = reallocarray(NULL, events_len, sizeof(epoll_t));
	if (!events)
	{
		fprintf(stderr, "Failed to realloc an array for epoll: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int ShutdownMultiplexer(void)
{
	// Close the EPoll handle.
	close(EpollHandle);
	
	// Free our events
	free(events);
	return 0;
}

extern socket_vec_t socketpool;
void ProcessSockets(void)
{
	if (socketpool.length > events_len)
	{
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
	
	for (int i = 0; i < total; ++i)
	{
		epoll_t *ev = &events[i];
		
		socket_t *s = FindSocket(ev->data.fd);
		if (!s)
			continue;
		
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
