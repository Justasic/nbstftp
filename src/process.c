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
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE 1
#include "process.h"
#include "misc.h"
#include "config.h"
#include "socket.h"
#include "filesystem.h"
#include <assert.h>
#include <errno.h>

// Process the incoming packet.
void ProcessPacket(client_t *c, void *buffer, size_t len)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
// 		Error(c, ERROR_ILLEGAL, "Invalid packet size");
		printf("Received an invalidly sized packet.\n");
		return;
	}
	
	assert(buffer);

	const packet_t *p = buffer;
	
	switch(ntohs(p->opcode))
	{
		case PACKET_DATA:
			printf("Got a data packet\n");
			if ((len + sizeof(packet_t) > MAX_PACKET_SIZE))
			{
				fprintf(stderr, "Received an oversided data packet! Terminating transfer.\n");
				Error(c, ERROR_ILLEGAL, "Sent an oversided packet.");
			}

			if ((len - sizeof(packet_t)) < 512)
				printf("Got end of data packet?\n");
			break;
		case PACKET_ERROR:
		{
			// icky! -- Cast the packet_t pointer to a uint8_t then increment 4 bytes, then cast
			// to a const char * and send to printf.
			const char *error = strndupa(((const char*)p) + sizeof(packet_t), 512);
			printf("Error: %s (%d)\n", error, ntohs(p->blockno));

			// Send an Acknowledgement packet.
			Acknowledge(c, 1);

			// For whatever reason, BIOSes send a WRQ packet to verify the
			// file exists and then cancels immediately upon receiving the
			// first packet. This makes the server freak the fuck out and
			// do things wrong. So here we deallocate if we run into any
			// kind of error.

                        if (c->sendingfile)
                        {
				printf("Aborting file transfer.\n");
				// Close the file and clean up.
				fclose(c->f);
				RemoveClient(c);
			}

			break;
		}
		case PACKET_ACK:
		{
			char *addr1 = inet_ntoa(c->s->addr.in.sin_addr);
			printf("Got Acknowledgement packet for block %d from %s\n", ntohs(p->blockno), addr1);
			
			if (c->sendingfile)
			{
				uint8_t buf[512];
				memset(buf, 0, sizeof(buf));
				size_t readlen = fread(buf, 1, sizeof(buf), c->f);
				
				c->currentblockno++;
				
				// Sending a file
				SendData(c, buf, readlen);
				
				// We're at the end of the file.
				if (MIN(512, readlen) != 512)
				{
					printf("Finished sending file\n");
					c->destroy = 1;
					break;
				}
			}

			break;
		}
		case PACKET_WRQ:
		{
			const char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);

			const char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);

			printf("Got write request packet for file \"%s\" in mode %s\n", filename, mode);
			Error(c, ERROR_UNDEFINED, "Operation not yet supported.");
			break;
		}
		case PACKET_RRQ:
		{
			// Get the filename and modes
			//
			// Since we dig only past the first value in the struct, we only
			// get the size of that first value (eg, the uint16_t)
			// Use strndupa to use the stack frame for temporary allocation with a
			// max length of 512 bytes. This will prevent buffer-overflow exploits (or so I hope)
			const char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);
			// This one is a bit weirder. We get the size of the uint16 like
			// we did above but also skip our filename string AND the remaining
			// null byte which strlen does not include.
			const char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
			
			// mode can be "netascii", "octet", or "mail" case insensitive.
			printf("Got read request packet: \"%s\" -> \"%s\"\n", filename, mode);

			// We don't support mail-mode
			if (!strcasecmp(mode, "mail"))
			{
				Error(c, ERROR_ILLEGAL, "Mail mode not supported by NBSTFTP");
				break;
			}

			int imode = strcasecmp(mode, "netascii");

			char tmp[(1 << 16)];
			sprintf(tmp, "%s/%s", config->directory, filename);

			if (!FileExists(tmp))
				Error(c, ERROR_NOFILE, "File %s does not exist on the filesystem.", tmp);

			FILE *f = fopen(tmp, (imode == 0 ? "rt" : "rb"));
			if (!f)
			{
				fprintf(stderr, "Failed to open file %s for sending: %s\n", tmp, strerror(errno));
				Error(c, ERROR_NOFILE, "Cannot open file: %s", strerror(errno));
				break;
			}

			printf("File \"%s\" is available, sending first packet\n", tmp);

			// file buffer
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));
			size_t readlen = fread(buf, 1, sizeof(buf), f);
			c->f = f;
			c->currentblockno = 1;
			c->sendingfile = 1;
			SendData(c, buf, readlen);

			break;
                }
		default:
			printf("Got unknown packet: %d\n", ntohs(p->opcode));
			// Unknown packets are ignored according to the RFC.
			//Error(c, ERROR_ILLEGAL, "Unknown packet");
			break;
	}
}
