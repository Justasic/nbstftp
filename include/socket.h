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
#include <stddef.h>
#include <stdint.h>
#include "packets.h"
#include "vec.h"
// Forward declare to prevent circular includes.
typedef struct client_s client_t;

void ProcessPacket(client_t *c, void *buffer, size_t len);

extern int BindToSocket(const char *addr, short port);

extern int InitializeSockets(void);
extern void ProcessSockets(void);
extern void ShutdownSockets(void);

enum
{
	SF_WRITABLE = 1,
	SF_READABLE = 2
};

typedef struct socket_s
{
	int fd;
	int type;
	uint32_t flags;
	socketstructs_t addr;
	char *bindaddr;
} socket_t;

typedef vec_t(socket_t*) socket_vec_t;

extern void SetSocketStatus(socket_t *s, int status);
