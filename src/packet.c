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

void SendData(client_t *c, void *data, size_t len)
{
	// Make sure the packet size does not exceed the max packet length.
        assert((len + sizeof(packet_t) <= MAX_PACKET_SIZE));
        assert(c);
	
	// Allocate a packet that is as big as the data + len
	packet_t *p = malloc(len + sizeof(packet_t));
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
	
	int sendlen = sendto(c->fd, p, len + sizeof(packet_t), 0, &c->addr.sa, sizeof(c->addr.in));
	if (sendlen == -1)
	{
		perror("sendto failed");
	}
	
	// Free our packet.
	free(p);
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
	
	int sendlen = sendto(c->fd, &p, sizeof(packet_t), 0, &c->addr.sa, sizeof(c->addr.sa));
	if (sendlen == -1)
	{
		perror("sendto failed");
	}
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
        
	size_t len = strlen(str);

        char *buf = malloc(len*2);
        va_list ap;
        va_start(ap, str);
        vsnprintf(buf, len*2, str, ap);
        va_end(ap);

        len = strlen(buf);
        buf = realloc(buf, len+1);
	
        // If your message is seriously bigger than 512 characters
        // then you need to rethink what is going on.
        // The end user doesn't need a god damn book because a file
        // doesn't exist or some shit.
        assert((len + sizeof(packet_t)) <= MAX_PACKET_SIZE);

	packet_t *p = malloc(len + sizeof(packet_t) + 1);
	p->opcode   = htons(PACKET_ERROR);
	p->blockno  = htons(errnum);
	
	// Fancy casting magic!
	uint8_t *pptr = ((uint8_t*)p) + sizeof(packet_t);
	strncpy((char*)pptr, buf, len);
	// guaranteed null-termination.
	pptr[len] = 0;

	socklen_t socklen = sizeof(c->addr.in);
	int sendlen = sendto(c->fd, p, len + sizeof(packet_t) + 1, 0, &c->addr.sa, socklen);
	if (sendlen == -1)
	{
		perror("sendto failed");
	}

	
	// Free our packet.
	free(p);
        free(buf);
}
