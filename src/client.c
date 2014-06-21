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
#include "client.h"
#include <assert.h>

client_t *front = NULL;

void AddClient(client_t *c)
{
	assert(c);
	
	if (!front)
	{
		front = c;
		c->next = NULL;
		c->prev = NULL;
		return;
	}
	
	
}

void RemoveClient(client_t *c)
{
	assert(c);
}

int CompareClients(client_t *c1, client_t *c2)
{
	assert(c1);
	assert(c2);
}

client_t *FindClient(socketstructs_t ss)
{
	// I felt like I was a nazi when I wrote this function...
	if (ss.sa.sa_family == AF_INET)
	{
		char *needle = inet_ntoa(sa.in.sin_addr);
		for (client_t *s = front; s; s = s->next)
		{
			char *haystack = inet_ntoa(s.addr.in.sin_addr), *addr2 = NULL;
			
			if (currentclient)
				addr2 = 
		}
	}
}
