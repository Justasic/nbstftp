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
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <netinet/in.h>

client_t *front = NULL, *end = NULL;

void AddClient(client_t *c)
{
	assert(c);
	
	if (!front)
	{
		front = c;
                end = front;
		c->next = NULL;
		c->prev = NULL;
		return;
	}
	
	c->prev = end;
        c->next = NULL;
        end->next = c;
        end = c;
}

client_t *FindOrAllocateClient(socket_t *s)
{
        client_t *found = FindClient(s);

        if (!found)
        {
		printf("Allocating client!\n");
                found = malloc(sizeof(client_t));
                memset(found, 0, sizeof(client_t));
//                 memcpy(&found->s->addr, &s->addr, sizeof(socketstructs_t));
                found->s = s;
                AddClient(found);
        }

        return found;
}

void RemoveClient(client_t *c)
{
	assert(c);

        if (c->prev)
                c->prev->next = c->next;

        if (c->next)
                c->next->prev = c->prev;

        if (c == end)
                end = c->prev;

        if (c == front)
                front = c->next;

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
	// I felt like I was a nazi when I wrote this function...
	if (s->addr.sa.sa_family == AF_INET)
        // Looking for an IPv4 address-based client
	{
		for (client_t *c = front; c; c = c->next)
		{
			if (c->s->fd == s->fd)
                                return c;
		}
	}
        else
        // Looking for an IPv6 address-based client
        {
               for (client_t *c = front; c; c = c->next)
               {
		       if (c->s->fd == s->fd)
                               return c;
               }
        }
        
        // Couldn't find the client, fall through to return NULL.
        return NULL;
}
