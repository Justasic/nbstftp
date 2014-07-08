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

#ifndef HAVE_KQUEUE
# error You probably shouldn't be trying to compile a kqueue multiplexer on a kqueue-unsupported platform. Try again.
#endif

#include "socket.h"
#include "misc.h"
#include "config.h"
#include "client.h"
#include "process.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

/* Not sure why these break but whatever. */
typedef unsigned short u_short;
typedef unsigned int u_int;

#include <sys/event.h>

// Make it easier
typedef struct kevent kevent_t;

static int KqueueHandle = -1;
static kevent_t *events = NULL, *changed = NULL;
static size_t events_len = 5, changed_len = 5;
static unsigned int changedSz = 0;
// Default kqueue idle time, will be changed on init
static struct timespec kqtime = {5, 0};

static inline kevent_t *GetChangeEvent(void)
{
	if (changedSz == changed_len)
	{
		printf("Resizing array to support changed_len\n");
		kevent_t *newptr = reallocarray(changed, changed_len * 2, sizeof(kevent_t));
		if (!newptr)
		{
			fprintf(stderr, "Failed to allocate more memory to watch events on more sockets! (%s)\n",
				strerror(errno));
			return NULL;
		}
		else
		{
			changed_len *= 2;
			changed = newptr;
			return NULL;
		}
	}
	
	return &changed[changedSz++];
}

int AddToMultiplexer(socket_t *s)
{
	// Set the socket as readable and add it to kqueue
	return SetSocketStatus(s, SF_READABLE);
}

int RemoveFromMultiplexer(socket_t *s)
{
	// It is easier to just set the socket status to 0, the if statements
	// in the next func below will take care of everything.
	return SetSocketStatus(s, 0);
}

int SetSocketStatus(socket_t *s, int status)
{
	assert(s);

	kevent_t *event;

	printf("Setting socket status %d (%d)\n", status, s->flags);

	if (status & SF_READABLE && !(s->flags & SF_READABLE))
	{
		printf("Setting readable (previously was not readbale)\n");
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	}

	if (!(status & SF_READABLE) && s->flags & SF_READABLE)
	{
		printf("Removing readable (previously was readable)\n");
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	}

	if (status & SF_WRITABLE && !(s->flags & SF_WRITABLE))
	{
		printf("Setting writable (previously was not writable)\n");
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	}

	if (!(status & SF_WRITABLE) && s->flags & SF_WRITABLE)
	{
		printf("Removing writable (previously was writable)\n");
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	}

	s->flags = status;

	return 0;
}

int InitializeMultiplexer(void)
{
	if ((KqueueHandle = kqueue()) < 0)
	{
		fprintf(stderr, "Unable to create kqueue handle: %s\n", strerror(errno));
		return -1;
	}
	
	events = reallocarray(NULL, events_len, sizeof(kevent_t));
	if (!events)
	{
		fprintf(stderr, "Failed to realloc an array for kevent: %s\n", strerror(errno));
		return -1;
	}
	
	changed = reallocarray(NULL, changed_len, sizeof(kevent_t));
	if (!changed)
	{
		fprintf(stderr, "Failed to realloc an array for kevent: %s\n", strerror(errno));
		return -1;
	}

	// Set kqueue idle timeout from config
	kqtime.tv_sec = config->readtimeout;
	
	return 0;
}

int ShutdownMultiplexer(void)
{
	close(KqueueHandle);
	free(changed);
	free(events);

	return 0;
}

extern socket_vec_t socketpool;
void ProcessSockets(void)
{
	if (socketpool.length > events_len)
	{
		kevent_t *newptr = reallocarray(events, events_len * 2, sizeof(kevent_t));
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
	printf("Entering kevent\n");
	
	int total = kevent(KqueueHandle, changed, changedSz, events, events_len, &kqtime);
	changedSz = 0;

	
	if (total == -1)
	{
		if (errno != EINTR)
		{
				fprintf(stderr, "Error processing sockets: %s\n", strerror(errno));
		}
		return;
	}
	
	for (int i = 0; i < total; ++i)
	{
		kevent_t *ev = &events[i];
		
		if (ev->flags & EV_ERROR)
			continue;
		
		socket_t *s = FindSocket(ev->ident);
		if (!s)
			continue;
		
		if (ev->flags & EV_EOF)
		{
			printf("Epoll error reading socket %d, destroying.\n", s->fd);
			DestroySocket(s, 1);
			continue;
		}
		
		// process socket read events.
		if (ev->filter & EVFILT_READ)
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
		if (ev->filter & EVFILT_WRITE && SendPackets() == -1)
		{
			printf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}
	}
}
