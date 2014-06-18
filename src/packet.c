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

void SendData(client_t client, void *data, size_t len)
{
	// Make sure the packet size does not exceed the max packet length.
	if ((len + sizeof(packet_t)) > MAX_PACKET_SIZE)
	{
		printf("Cannot copy %lu bytes into DATA packet: packet too large!\n", len + sizeof(packet_t));
		exit(EXIT_FAILURE);
	}
	
	// Allocate a packet that is as big as the data + len
	packet_t *p = malloc(len + sizeof(packet_t));
	p->opcode   = htons(PACKET_DATA);
	p->blockno  = htons(client.currentblockno++);
	
	// Cast the pointer, move to the end of the struct,
	// then copy our data, this will mean we have a full
	// packet struct.
	//
	//     2 bytes     2 bytes      n bytes
	//     ----------------------------------
	//     | Opcode |   Block #  |   Data   |
	//     ----------------------------------
	//
	uint8_t *pptr = (uint8_t*)p;
	pptr += sizeof(packet_t);
	memcpy(pptr, data, len);
	
	socklen_t socklen = sizeof(client.addr.in);
	int sendlen = sendto(client.fd, pptr, len + sizeof(packet_t), 0, &client.addr.sa, socklen);
	if (sendlen == -1)
	{
		perror("sendto failed");
		exit(EXIT_FAILURE);
	}
	
	// Free our packet.
	free(p);
}

void Acknowledge(client_t client, uint16_t blockno)
{
	//
	//     2 bytes     2 bytes
	//     -----------------------
	//     | Opcode |   Block #  |
	//     -----------------------
	//
	packet_t p;
	p.opcode = PACKET_ACK;
	p.blockno = blockno;
	
	socklen_t socklen = sizeof(client.addr.in);
	int sendlen = sendto(client.fd, &p, sizeof(packet_t), 0, &client.addr.sa, socklen);
	if (sendlen == -1)
	{
		perror("sendto failed");
		exit(EXIT_FAILURE);
	}
}

void Error(client_t client, const uint16_t errnum, const char *str)
{
	//
	//     2 bytes     2 bytes      string    1 byte
	//     -------------------------------------------
	//     | Opcode |  ErrorCode |   ErrMsg   |   0  |
	//     -------------------------------------------
	//
	size_t len = strlen(str);
	
	if ((len + sizeof(packet_t)) > MAX_PACKET_SIZE)
	{
		printf("Cannot copy %lu bytes into ERROR packet: packet too large!\n", len + sizeof(packet_t));
		exit(EXIT_FAILURE);
	}
	
	packet_t *p = malloc(len + sizeof(packet_t));
	p->opcode   = htons(PACKET_ERROR);
	p->blockno  = htons(errnum);
	
	// Fancy casting magic!
	uint8_t *pptr = (uint8_t*)p;
	pptr += sizeof(packet_t);
	strncpy((char*)pptr, str, len);
	// guaranteed null-termination.
	pptr[len] = 0;
	
	socklen_t socklen = sizeof(client.addr.in);
	int sendlen = sendto(client.fd, pptr, len + sizeof(packet_t), 0, &client.addr.sa, socklen);
	if (sendlen == -1)
	{
		perror("sendto failed");
		exit(EXIT_FAILURE);
	}
	
	// Free our packet.
	free(p);
}
