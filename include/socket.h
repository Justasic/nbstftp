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

extern int BindToSocket(const char *addr, short port);

extern int InitializeSockets(void);
extern void ProcessSockets(void);
extern void ShutdownSockets(void);

typedef struct socket_s
{
	int fd;
	int type;
	uint32_t flags;
	socketstructs_t addr;
	char *bindaddr;
} socket_t;

typedef vec_t(socket_t) socket_vec_t;
extern socket_vec_t socketpool;

extern short GetPort(socket_t s);
extern void DestroySocket(socket_t s, uint8_t close);

extern int AddSocket(int fd, const char *addr, int type, socketstructs_t saddr, uint8_t binding, socket_t *s);
extern int FindSocket(int fd, socket_t *s);

extern void QueuePacket(client_t *c, packet_t *p, size_t len, uint8_t allocated);
extern int SendPackets(socket_t s);
extern int ReceivePackets(socket_t s);
extern const char *GetAddress(socketstructs_t saddr);