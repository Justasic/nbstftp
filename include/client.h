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
#pragma once
#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>
#include "packets.h"

typedef struct client_s
{
	socketstructs_t addr;
	int fd;
        // Status variables
	uint16_t currentblockno;
        uint8_t gotack, sendingfile;

        // The last packet of data we sent.
        packet_t *lpacket;
        size_t lpacketlen;

        // Current file we're sending
        FILE *f;
        // Next and previous clients
	struct client_s *next, *prev;
} client_t;

// Add a client to the existing client list
extern void AddClient(client_t *c);
// Remove a client from the list
extern void RemoveClient(client_t *c);
// Compare two clients and check if they're equal
extern int CompareClients(client_t *c1, client_t *c2);
// Find a client based on their socket structure.
extern client_t *FindClient(socketstructs_t sa);
// Either find a client or allocate a new one, also adds it to the linked list.
extern client_t *FindOrAllocateClient(socketstructs_t sa, int fd);

