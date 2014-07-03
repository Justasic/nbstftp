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
#include "packets.h"

// Whatever, include it all. Figure it out later.
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include "client.h"
#include "misc.h"
#include "socket.h"

// Queue packets for sending -- internal function
static void QueuePacket(client_t *c, packet_t *p, size_t len, uint8_t allocated)
{
	printf("Queuing packet %d len %zu\n", p->opcode, len);
	// We're adding another packet.
	// Try and keep up with the required queue
	if (++c->queuelen >= c->alloclen)
	{
		printf("Expanding queue to %zu items (%zu bytes total)\n", c->queuelen, c->queuelen * sizeof(packetqueue_t));
		packetqueue_t **newqueue = reallocarray(c->packetqueue, sizeof(packetqueue_t), c->queuelen);
		if (!newqueue)
		{
			// This should cause a resend of the packet or a timeout.
			fprintf(stderr, "Failed to allocate more slots in packet queue, discarding packet! (%s)\n",
				strerror(errno));
			if (allocated)
				free(p);
		}
		c->alloclen = c->queuelen;
		c->packetqueue = newqueue;
	}
	
	packetqueue_t *pack = nmalloc(sizeof(packetqueue_t));
	pack->p = p;
	pack->len = len;
	pack->allocated = allocated;
	
	c->packetqueue[c->queuelen-1] = pack;
	
	printf("Setting socket as writable\n");
	// We're ready to write.
	SetSocketStatus(c->s, SF_WRITABLE | SF_READABLE);
}

// Send packets out the socket, this will be called by the epoll
// system in socket.c.
int SendPackets(void)
{
	client_t *c = NULL;
	int i;
	
	vec_foreach(&clientpool, c, i)
	{
		for (size_t i = 0, end = c->queuelen; i < end; ++i, c->queuelen--)
		{
			packetqueue_t *packet = c->packetqueue[i];
			printf("Packet %d length %zu\n", ntohs(packet->p->opcode), packet->len);
			
			printf("Responding on socket %d\n", c->s->fd);
			
			int sendlen = sendto(c->s->fd, packet->p, packet->len, 0, &c->s->addr.sa, sizeof(c->s->addr.sa));
			if (sendlen == -1)
			{
				perror("sendto failed");
				return -1;
			}
			
			printf("Sent packet of length %d\n", sendlen);
			
			// Free the packet structure
			if (packet->allocated)
				free(packet->p);
			
			// We sent all the packets we want.
		}
		printf("Setting socket as readable\n");
		SetSocketStatus(c->s, SF_READABLE);
	}

	return 0;
}

void SendData(client_t *c, void *data, size_t len)
{
	// Make sure the packet size does not exceed the max packet length.
        assert((len + sizeof(packet_t) <= MAX_PACKET_SIZE));
        assert(c && data);
	
	// Allocate a packet that is as big as the data + len
	packet_t *p = nmalloc(len + sizeof(packet_t));
	p->opcode   = htons(PACKET_DATA);
	p->blockno  = htons(c->currentblockno);
	
	// Cast the pointer, move to the end of the struct,
	// then copy our data, this will mean we have a full
	// packet struct.
	//
	//     2 bytes     2 bytes      n bytes
	//     ----------------------------------
	//     | Opcode |   Block #  |   Data   |
	//     ----------------------------------
	//
	uint8_t *pptr = ((uint8_t*)p) + sizeof(packet_t);
	memcpy(pptr, data, len);
	
	// Queue our packet for sending when EPoll comes around to send.
	QueuePacket(c, p, len + sizeof(packet_t), 1);
	
// 	int sendlen = sendto(c->fd, p, len + sizeof(packet_t), 0, &c->addr.sa, sizeof(c->addr.in));
// 	if (sendlen == -1)
// 		perror("sendto failed");
// 	
// 	// Free our packet.
// 	free(p);
}

void Acknowledge(client_t *c, uint16_t blockno)
{
        assert(c);
	//
	//     2 bytes     2 bytes
	//     -----------------------
	//     | Opcode |   Block #  |
	//     -----------------------
	//
	packet_t p;
	p.opcode = htons(PACKET_ACK);
	p.blockno = htons(blockno);
	
	QueuePacket(c, &p, sizeof(packet_t), 0);
	
// 	int sendlen = sendto(c->fd, &p, sizeof(packet_t), 0, &c->addr.sa, sizeof(c->addr.sa));
// 	if (sendlen == -1)
// 	{
// 		perror("sendto failed");
// 	}
}

__attribute__((format(printf, 3, 4)))
void Error(client_t *c, const uint16_t errnum, const char *str, ...)
{
	//
	//     2 bytes     2 bytes      string    1 byte
	//     -------------------------------------------
	//     | Opcode |  ErrorCode |   ErrMsg   |   0  |
	//     -------------------------------------------
	//

	assert(c);

	char *buf = NULL;
	va_list ap;
	va_start(ap, str);
	size_t len = vasprintf(&buf, str, ap);
	va_end(ap);
	
	if (len == -1)
	{
		fprintf(stderr, "ERROR: cannot format error string: %s\n", strerror(errno));
		return;
	}

	// If your message is seriously bigger than 512 characters
	// then you need to rethink what is going on.
	// The end user doesn't need a god damn book because a file
	// doesn't exist or some shit.
	assert((len + sizeof(packet_t)) <= MAX_PACKET_SIZE);

	packet_t *p = nmalloc(len + sizeof(packet_t) + 1);
	p->opcode   = htons(PACKET_ERROR);
	p->blockno  = htons(errnum);
	
	// Fancy casting magic!
	uint8_t *pptr = ((uint8_t*)p) + sizeof(packet_t);
	strncpy((char*)pptr, buf, len);
	// guaranteed null-termination.
	pptr[len] = 0;
	
	QueuePacket(c, p, len + sizeof(packet_t) + 1, 1);

// 	socklen_t socklen = sizeof(c->addr.in);
// 	int sendlen = sendto(c->fd, p, len + sizeof(packet_t) + 1, 0, &c->addr.sa, socklen);
// 	if (sendlen == -1)
// 		perror("sendto failed");
// 
// 	// Free our packet.
// 	free(p);
        free(buf);
}
