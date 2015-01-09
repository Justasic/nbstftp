/*
 * Copyright (c) 2014-2015, Justin Crawford <Justasic@gmail.com>
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
# error "You probably shouldn't be trying to compile a kqueue multiplexer on a kqueue-unsupported platform. Try again."
#endif

#include "multiplexer.h"
#include "misc.h"
#include "config.h"
#include "client.h"
#include "process.h"
#include "vec.h"
#include "module.h"

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

static int KqueueHandle = -1;
// Make it easier
typedef struct kevent kevent_t;
vec_t(kevent_t) events, changed;
// Default kqueue idle time, will be changed on init
static struct timespec kqtime = {5, 0};

static inline kevent_t *GetChangeEvent(void)
{
	if (changed.length == changed.capacity)
		vec_reserve(&changed, changed.length * 2);

	return &(changed.data[changed.length++]);
}

int AddToMultiplexer(socket_t *s)
{
	// Set the socket as readable and add it to kqueue
	return SetSocketStatus(s, SF_READABLE);
}

int RemoveFromMultiplexer(socket_t s)
{
	// It is easier to just set the socket status to 0, the if statements
	// in the next func below will take care of everything.
	return SetSocketStatus(&s, 0);
}

int SetSocketStatus(socket_t *s, int status)
{
	assert(s);

	kevent_t *event;

	if (status & SF_READABLE && !(s->flags & SF_READABLE))
	{
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
	}

	if (!(status & SF_READABLE) && s->flags & SF_READABLE)
	{
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	}

	if (status & SF_WRITABLE && !(s->flags & SF_WRITABLE))
	{
		event = GetChangeEvent();
		EV_SET(event, s->fd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
	}

	if (!(status & SF_WRITABLE) && s->flags & SF_WRITABLE)
	{
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

	// Initialize
	vec_init(&events);
	vec_init(&changed);

	// Reserve some initial space
	vec_reserve(&events, 5);
	vec_reserve(&changed, 5);

	// Reinitialize the arrays.
	memset(events.data, 0, sizeof(kevent_t) * 5);
	memset(changed.data, 0, sizeof(kevent_t) * 5);

	// Set kqueue idle timeout from config
	kqtime.tv_sec = config->readtimeout;

	return 0;
}

int ShutdownMultiplexer(void)
{
	close(KqueueHandle);

	vec_deinit(&events);
	vec_deinit(&changed);

	return 0;
}

void ProcessSockets(void)
{
	if (socketpool.length > events.capacity)
		vec_reserve(&events, socketpool.length * 2);

	bprintf("Entering kevent\n");

	int total = kevent(KqueueHandle, &vec_first(&changed), changed.length, &vec_first(&events), events.capacity, &kqtime);

	// Reset the changed count.
	vec_clear(&changed);

	if (total == -1)
	{
		if (errno != EINTR)
			fprintf(stderr, "Error processing sockets: %s\n", strerror(errno));
		return;
	}

	for (int i = 0; i < total; ++i)
	{
		kevent_t *ev = &(events.data[i]);

		if (ev->flags & EV_ERROR)
			continue;

		socket_t s;
		if (FindSocket(ev->ident, &s) == -1)
		{
			bfprintf(stderr, "Unknown FD in multiplexer: %d\n", ev->ident);
			// We don't know what socket this is. Someone added something
			// stupid somewhere so shut this shit down now.
			// We have to create a temporary socket_t object to remove it
			// from the multiplexer, then we can close it.
			socket_t tmp = { ev->data.fd, 0, 0, 0, 0 };
			RemoveFromMultiplexer(tmp);
			close(ev->ident);
			continue;
		}

		// Call our event.
		CallEvent(EV_SOCKETACTIVITY, &s);

		if (ev->flags & EV_EOF)
		{
			bprintf("Kqueue error reading socket %d, destroying.\n", s.fd);
			DestroySocket(s, 1);
			continue;
		}

		// Process socket write events
		if (ev->filter & EVFILT_WRITE && SendPackets(s) == -1)
		{
			bprintf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}

		// process socket read events.
		if (ev->filter & EVFILT_READ && ReceivePackets(s) == -1)
		{
			bprintf("Destorying socket due to receive failure!\n");
			DestroySocket(s, 1);
		}
	}
}
