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
# error "You probably shouldn't be trying to compile an epoll multiplexer on a epoll-unsupported platform. Try again."
#endif

#include "multiplexer.h"
#include "misc.h"
#include "config.h"
#include "client.h"
#include "process.h"
#include "vec.h"

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
static vec_t(epoll_t) events;

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

int RemoveFromMultiplexer(socket_t s)
{
	epoll_t ev;
	memset(&ev, 0, sizeof(epoll_t));
	ev.data.fd = s.fd;
	
	if (epoll_ctl(EpollHandle, EPOLL_CTL_DEL, ev.data.fd, &ev) == -1)
	{
		fprintf(stderr, "Unable to remove fd %d from epoll: %s\n", s.fd, strerror(errno));
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
	
	if (EpollHandle == -1)
		return -1;
	
	vec_init(&events);
	
	// Default of 5 events, it will expand if we get more sockets
	vec_reserve(&events, 5);
	
	if (errno == ENOMEM)
		return -1;
	
	return 0;
}

int ShutdownMultiplexer(void)
{
	// Close the EPoll handle.
	close(EpollHandle);
	
	// Free our events
	vec_deinit(&events);
	
	return 0;
}

void ProcessSockets(void)
{
	if (socketpool.length >= events.capacity)
	{
		printf("Reserving more space for events\n");
		vec_reserve(&events, socketpool.length * 2);
	}
	
	printf("Entering epoll_wait\n");
	
	int total = epoll_wait(EpollHandle, &vec_first(&events), events.capacity, config->readtimeout * 1000);
	
	if (total == -1)
	{
		if (errno != EINTR)
			fprintf(stderr, "Error processing sockets: %s\n", strerror(errno));
		return;
	}
	
	for (int i = 0; i < total; ++i)
	{
		epoll_t *ev = &(events.data[i]);
		
		socket_t s;
		if (FindSocket(ev->data.fd, &s) == -1)
		{
			fprintf(stderr, "Unknown FD in multiplexer: %d\n", ev->data.fd);
			// We don't know what socket this is. Someone added something
			// stupid somewhere so shut this shit down now.
			// We have to create a temporary socket_t object to remove it
			// from the multiplexer, then we can close it.
			socket_t tmp = { ev->data.fd, 0, 0, 0, 0 };
			RemoveFromMultiplexer(tmp);
			close(ev->data.fd);
			continue;
		}
		
		if (ev->events & (EPOLLHUP | EPOLLERR))
		{
			printf("Epoll error reading socket %d, destroying.\n", s.fd);
			DestroySocket(s, 1);
			continue;
		}
		
		// process socket read events.
		if (ev->events & EPOLLIN && ReceivePackets(s) == -1)
		{
			printf("Destorying socket due to receive failure!\n");
			DestroySocket(s, 1);
		}
		
		// Process socket write events
		if (ev->events & EPOLLOUT && SendPackets(s) == -1)
		{
			printf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}
	}
}
