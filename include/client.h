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
#include "vec.h"
// #include "socket.h"
typedef struct socket_s socket_t;

typedef struct packetqueue_s
{
	packet_t *p;
	size_t len;
	uint8_t allocated;
} packetqueue_t;

typedef struct client_s
{
	socket_t *s;
        // Status variables
	uint16_t currentblockno;
        uint8_t gotack, sendingfile;
	
	// Buffered packets. May use it later when a packet has not
	// been received yet and we need to resend.
	packetqueue_t **packetqueue;
	size_t queuelen, alloclen;

        // Current file we're sending
        FILE *f;
} client_t;

typedef vec_t(client_t*) client_vec_t;
// Our client pool
extern client_vec_t clientpool;

// Add a client to the existing client list
extern void AddClient(client_t *c);
// Remove a client from the list
extern void RemoveClient(client_t *c);
// Compare two clients and check if they're equal
extern int CompareClients(client_t *c1, client_t *c2);
// Find a client based on their socket structure.
extern client_t *FindClient(socket_t *s);
// Either find a client or allocate a new one, also adds it to the linked list.
extern client_t *FindOrAllocateClient(socket_t *s);
extern void DeallocateClients(void);

