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
// Recursive-include fix
typedef struct client_s client_t;
#include "client.h"
#include "socket.h"
#include "vec.h"
#include "misc.h"
#include <assert.h>
#include <errno.h>

client_vec_t clientpool;

void AddClient(client_t *c)
{
	assert(c);
	
	vec_push(&clientpool, c);
	// Make sure we could add the socket.
	if (errno == ENOMEM)
	{
		fprintf(stderr, "Failed to add client to client pool!\n");
		RemoveClient(c);
	}
}

client_t *FindOrAllocateClient(socket_t *cs)
{
	assert(cs);
	
	client_t *found = FindClient(cs);

	if (!found)
	{
		printf("*** Allocating client!\n");
		found = nmalloc(sizeof(client_t));
//                 memcpy(&found->s->addr, &s->addr, sizeof(socketstructs_t));
		// Make a permanent socket which gets added to the socket vector.
		found->s = AddSocket(cs->fd, NULL, cs->type, cs->addr, 0);
		AddClient(found);
	}

	return found;
}

void RemoveClient(client_t *c)
{
	assert(c);

	// Remove the socket from the socket pool
// 	DestroySocket(c->s, 0);
	
	// Remove the client from the client pool
	vec_remove(&clientpool, c);

	// Delete the client
	free(c);
}

int CompareClients(client_t *c1, client_t *c2)
{
	assert(c1);
	assert(c2);

	assert(c1->s->addr.sa.sa_family == AF_INET ||
		c1->s->addr.sa.sa_family == AF_INET6);
	assert(c2->s->addr.sa.sa_family == AF_INET ||
		c2->s->addr.sa.sa_family == AF_INET6);
	
	socket_t *s1 = c1->s, *s2 = c2->s;

        // First, compare address types, if they're not a match then
        // obviously the clients are very different.
	if (s1->addr.sa.sa_family != s2->addr.sa.sa_family)
                return 0;

        // Now compare the addresses. IPv4 first
	if (s1->addr.sa.sa_family == AF_INET)
        {
		return memcmp(&s1->addr.in.sin_addr,
			      &s2->addr.in.sin_addr,
                              sizeof(struct in_addr)) == 0;
        }
        else
        {
		return memcmp(&s1->addr.in6.sin6_addr,
			      &s2->addr.in6.sin6_addr,
			      sizeof(struct in6_addr)) == 0;
        }

        // (should) never be reached but compilers whine about this not
        // being here so here it is.
        return 0;
}

client_t *FindClient(socket_t *s)
{
	assert(s);
	
// 	printf("Finding client for socket %d\n", s->fd);
	
	for (int idx = 0; idx < (&clientpool)->length; idx++)
		if ((&clientpool)->data[idx]->s->fd == s->fd)
		{
			client_t *c = (&clientpool)->data[idx];
			if (GetPort(c->s) == GetPort(s))
			{
// 				printf(" Found\n");
				return c;
			}
		}
        
//         printf(" Not found\n");
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
		if (c->queuelen)
		{
			printf("Not sending %zu packets to sock %d due to shutdown\n", c->queuelen, c->s->fd);
			for (size_t i = 0, end = c->queuelen; i < end; ++i, c->queuelen--)
			{
				packetqueue_t *packet = c->packetqueue[i];
				
				// Free the packet structure
				if (packet->allocated)
					free(packet->p);
			}
		}
		
		// Deallocate the client
		free(c->packetqueue);
		free(c);
	}
	
	// Deallocate the client pool
	vec_deinit(&clientpool);
}
