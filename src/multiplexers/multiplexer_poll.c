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

#ifndef HAVE_POLL
# error "You probably shouldn't be trying to compile an poll multiplexer on a poll-unsupported platform. Try again."
#endif

#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "multiplexer.h"
#include "misc.h"
#include "vec.h"
#include "config.h"
#include "client.h"
#include "process.h"
#include "module.h"

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

int RemoveFromMultiplexer(socket_t s)
{
	// Find our socket
	for (int idx = 0; idx < events.length; idx++)
	{
		if (events.data[idx].fd == s.fd)
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
	bprintf("Entering poll\n");

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

		socket_t s;
		if (FindSocket(ev->fd, &s) == -1)
		{
			bfprintf(stderr, "Unknown FD in multiplexer: %d\n", ev->fd);
			// We don't know what socket this is. Someone added something
			// stupid somewhere so shut this shit down now.
			// We have to create a temporary socket_t object to remove it
			// from the multiplexer, then we can close it.
			socket_t tmp = { ev->fd, 0, 0, 0, 0 };
			RemoveFromMultiplexer(tmp);
			close(ev->fd);
			continue;
		}

		// Call our event.
		CallEvent(EV_SOCKETACTIVITY, &s);

		if (ev->revents & (POLLERR | POLLRDHUP))
		{
			bprintf("Epoll error reading socket %d, destroying.\n", s.fd);
			DestroySocket(s, 1);
			continue;
		}

		if (ev->revents & POLLOUT && SendPackets(s) == -1)
		{
			bprintf("Destorying socket due to send failure!\n");
			DestroySocket(s, 1);
		}

		if (ev->revents & POLLIN && ReceivePackets(s) == -1)
		{
			bprintf("Destorying socket due to receive failure!\n");
			DestroySocket(s, 1);
		}
	}
}
