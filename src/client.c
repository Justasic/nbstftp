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
// Recursive-include fix
typedef struct client_s client_t;
#include "client.h"
#include "socket.h"
#include "vec.h"
#include "misc.h"
#include "module.h"
#include <assert.h>
#include <errno.h>
#include <time.h>

client_vec_t clientpool;

void AddClient(client_t *c)
{
	assert(c);

	vec_push(&clientpool, c);

	vec_init(&c->packetqueue_vec);
	// Make sure we could add the socket.
	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to add client to client pool!\n");
		RemoveClient(c);
	}

	// The transfer ID is given as the port.
	c->tid = -GetPort(c->s);
}

client_t *FindOrAllocateClient(socket_t cs)
{
	client_t *found = FindClient(cs);

	if (!found)
	{
		found = nmalloc(sizeof(client_t));
		// Make a semi-permanent socket which gets added to the socket vector.
		if (AddSocket(cs.fd, NULL, cs.type, cs.addr, 0, &found->s) == -1)
		{
			free(found);
			return NULL;
		}

		vec_init(&found->packetqueue_vec);
		if (errno == ENOMEM)
		{
			fprintf(stderr, "Failed to allocate packet queue!\n");
			return NULL;
		}

		AddClient(found);
	}

	return found;
}

void RemoveClient(client_t *c)
{
	assert(c);
	packetqueue_t pq;
	int idx;

	// Remove the socket from the socket pool
	DestroySocket(c->s, 0);

	// Remove the client from the client pool
	vec_remove(&clientpool, c);

	// Free any remaining packets that are in the packet queue
	vec_foreach(&c->packetqueue_vec, pq, idx)
	{
		if (pq.allocated)
			free(pq.p);
	}

	// Destroy the packet queue
	vec_deinit(&c->packetqueue_vec);

	// Destroy any allocated resend packets
	if (c->lastpacket.allocated)
		free(c->lastpacket.p);

	// Delete the client
	free(c);
}



int CompareClients(client_t *c1, client_t *c2)
{
	assert(c1);
	assert(c2);

	return c1->tid == c2->tid;
}

client_t *FindClient(socket_t s)
{
	for (int idx = 0; idx < (&clientpool)->length; idx++)
	{
		if ((&clientpool)->data[idx]->s.fd == s.fd)
		{
			client_t *c = (&clientpool)->data[idx];
			if (c->tid == -GetPort(s))
				return c;
		}
	}

	// Couldn't find the client, fall through to return NULL.
	return NULL;
}

void DeallocateClients(void)
{
	client_t *c = NULL;
	int i;

	// Deallocate any remaining packets and the client's queue.
	vec_foreach(&clientpool, c, i)
	{
		vec_deinit(&c->packetqueue_vec);
		free(c);
	}

	// Deallocate the client pool
	vec_deinit(&clientpool);
}


void CheckClients(void)
{
	client_t *c = NULL;
	int i;

	vec_foreach(&clientpool, c, i)
	{
		if (c->waiting != 0 && c->nextresend <= time(NULL))
		{
			if (c->waiting++ >= 3)
			{
				printf("Found client on socket %d taking too long... Terminating transfer.\n", c->s.fd);
				RemoveClient(c);
				continue;
			}

			struct { client_t *c; const packet_t *p; size_t len; } ev = { c, c->lastpacket.p, c->lastpacket.len };
			CallEvent(EV_RESEND, &ev);

			bprintf("Resending last packet, retry %d/3\n", c->waiting);
			// Resend the packet
			QueuePacket(c, c->lastpacket.p, c->lastpacket.len, 2);
			//c->nextresend = time(NULL) + 5;
		}
	}
}
