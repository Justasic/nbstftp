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
#include "process.h"
#include "misc.h"
#include "config.h"
#include "socket.h"
#include "filesystem.h"
#include "module.h"
#include <assert.h>
#include <errno.h>
#include "sysconf.h"
#include <strings.h>
#include <unistd.h>

// NOTE:
// This file is a bit of a mess but it works for now.
// The main thing about this file is that every packet received
// goes here and is handled here.

// Process the incoming packet.
void ProcessPacket(client_t *c, void *buffer, size_t len)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
		printf("Received an invalidly sized packet.\n");
		return;
	}

	assert(buffer);

	const packet_t *p = buffer;

	switch(ntohs(p->opcode))
	{
		case PACKET_DATA:
		{
			c->waiting = 0;
			if (c->lastpacket.allocated == 2)
			{
				free(c->lastpacket.p);
				c->lastpacket.allocated = c->lastpacket.len = 0;
			}

			dprintf("Got a data packet\n");
			
			struct { const packet_t *p; client_t *c; } ev = { p, c };
			CallEvent(EV_DATA_PACKET, &ev);

			// If we're sending a file then write the next block
			// otherwise, just ignore it because it's not ours.
			if (c->sendingfile)
			{
				c->currentblockno++;
				c->actualblockno++;
				
				size_t flen = fwrite(((uint8_t*)p) + sizeof(packet_t), 1, len - sizeof(packet_t), c->f);
				
				char *tmp2 = stringify(" (Actually %zu)", c->actualblockno);
				printf("Wrote block %d%s of length %zu (%s transferred)\r",
				       ntohs(p->blockno), ntohs(p->blockno) == c->actualblockno ? "" : tmp2,
				       flen, GetBlockSize(c->actualblockno));
				free(tmp2);

				Acknowledge(c, ntohs(p->blockno));

				if ((len - sizeof(packet_t)) < 512)
				{
					printf("Got end of data packet\n");
					// Notify on the sending of a packet that this needs to be removed.
					c->destroy = 1;
				}
			}
			break;
		}
		case PACKET_ERROR:
		{
			c->waiting = 0;
			if (c->lastpacket.allocated == 2)
			{
				free(c->lastpacket.p);
				c->lastpacket.allocated = c->lastpacket.len = 0;
			}
			// icky! -- Cast the packet_t pointer to a uint8_t then increment 4 bytes, then cast
			// to a const char * and send to printf.
#ifdef HAVE_STRNDUPA
			char *error = strndupa(((const char*)p) + sizeof(packet_t), 512);
#else
			char *error = strndup(((const char*)p) + sizeof(packet_t), 512);
#endif
			
			struct { const packet_t *p; client_t *c; } ev = { p, c };
			CallEvent(EV_ERROR_PACKET, &ev);
			
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
				c->destroy = 1;
			}

#ifndef HAVE_STRNDUPA
			free(error);
#endif

			break;
		}
		case PACKET_ACK:
		{
			c->waiting = 0;
			if (c->lastpacket.allocated == 2)
			{
				free(c->lastpacket.p);
				c->lastpacket.allocated = c->lastpacket.len = 0;
			}
			
			char *tmp2 = stringify(" (Actually %zu)", c->actualblockno);
			printf("Got Acknowledgement packet for block %d%s from %s (%s transferred)\r",
			       ntohs(p->blockno), ntohs(p->blockno) == c->actualblockno ? "" : tmp2,
			       GetAddress(c->s.addr), GetBlockSize(c->actualblockno));
			free(tmp2);
			
			struct { const packet_t *p; client_t *c; } ev = { p, c };
			CallEvent(EV_ACK_PACKET, &ev);

			if (c->sendingfile)
			{
				uint8_t buf[512];
				memset(buf, 0, sizeof(buf));
				size_t readlen = fread(buf, 1, sizeof(buf), c->f);

				c->currentblockno++;
				c->actualblockno++;

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
			c->waiting = 0;
			if (c->lastpacket.allocated == 2)
			{
				free(c->lastpacket.p);
				c->lastpacket.allocated = c->lastpacket.len = 0;
			}
#ifdef HAVE_STRNDUPA
			char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);
			char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
#else
			char *filename = strndup(((const char *)p) + sizeof(uint16_t), 512);
			char *mode = strndup(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
#endif

			printf("Got write request packet for file \"%s\" in mode %s\n", filename, mode);

			// We don't support mail-mode
			if (!strcasecmp(mode, "mail"))
			{
				Error(c, ERROR_ILLEGAL, "Mail mode not supported by NBSTFTP");
				break;
			}

			int imode = strcasecmp(mode, "netascii");
			
			if (config->fixpath)
				FixPath(filename);

			char *tmp = NULL;
			asprintf(&tmp, "%s/%s", config->directory, filename);
			
			// Something's fucked. We're out of memory, try and abort peacefully.
			if (!tmp)
			{
				Error(c, ERROR_UNDEFINED, "Out of Memory");
				break;
			}
			
			dprintf("Checking we can open file \"%s\"\n", tmp);
			
			// Check that we have access to that file
			if (access(tmp, W_OK) == -1 || access(config->directory, W_OK) == -1)
			{
				if (errno == EACCES)
				{
					Error(c, ERROR_NOUSER, "Cannot access file: %s", strerror(errno));
					goto end;
				}
			}

			dprintf("Opening file %s as %s\n", tmp, imode == 0 ? "wt" : "wb");
			FILE *f = fopen(tmp, imode == 0 ? "wt" : "wb");
			if (!f)
			{
				Error(c, ERROR_NOFILE, "Cannot write file: %s", strerror(errno));
				goto end;
			}

			dprintf("File %s is available for write, writing first packet...\n", tmp);

			c->f = f;
			c->currentblockno = 1;
			c->actualblockno = 1;
			c->sendingfile = 1;
			
			struct { const packet_t *p; client_t *c; char *filename, *mode, *path; }
				ev = { p, c, filename, mode, tmp };
			CallEvent(EV_NEWWRITEREQUEST, &ev);

			// Acknowledge our transfer request
			Acknowledge(c, 0);

end:
#ifndef HAVE_STRNDUPA
			free(filename);
			free(mode);
#endif
			free(tmp);
			break;
		}
		case PACKET_RRQ:
		{
			c->waiting = 0;
			if (c->lastpacket.allocated == 2)
			{
				free(c->lastpacket.p);
				c->lastpacket.allocated = c->lastpacket.len = 0;
			}
#ifdef HAVE_STRNDUPA
			// Get the filename and modes
			//
			// Since we dig only past the first value in the struct, we only
			// get the size of that first value (eg, the uint16_t)
			// Use strndupa to use the stack frame for temporary allocation with a
			// max length of 512 bytes. This will prevent buffer-overflow exploits (or so I hope)
			char *filename = strndupa(((const char *)p) + sizeof(uint16_t), 512);
			// This one is a bit weirder. We get the size of the uint16 like
			// we did above but also skip our filename string AND the remaining
			// null byte which strlen does not include.
			char *mode = strndupa(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
#else
			char *filename = strndup(((const char *)p) + sizeof(uint16_t), 512);
			char *mode = strndup(((const char *)p) + (sizeof(uint16_t) + strnlen(filename, 512) + 1), 512);
#endif

			// mode can be "netascii", "octet", or "mail" case insensitive.
			printf("Got read request packet: \"%s\" -> \"%s\"\n", filename, mode);

			// We don't support mail-mode
			if (!strcasecmp(mode, "mail"))
			{
				Error(c, ERROR_ILLEGAL, "Mail mode not supported by NBSTFTP");
				break;
			}

			int imode = strcasecmp(mode, "netascii");
			
			if (config->fixpath)
				FixPath(filename);

			char *tmp = NULL;
			asprintf(&tmp, "%s/%s", config->directory, filename);

			if (!FileExists(tmp))
				Error(c, ERROR_NOFILE, "File %s does not exist on the filesystem.", tmp);

			FILE *f = fopen(tmp, (imode == 0 ? "rt" : "rb"));
			if (!f)
			{
				fprintf(stderr, "Failed to open file %s for sending: %s\n", tmp, strerror(errno));
				Error(c, ERROR_NOFILE, "Cannot open file: %s", strerror(errno));
				break;
			}

			fseek(f, 0, SEEK_END);
			size_t len = ftell(f);
			rewind(f);

			dprintf("File \"%s\" is %s long, sending first packet\n", tmp, SizeReduce(len));

			// file buffer
			uint8_t buf[512];
			memset(buf, 0, sizeof(buf));
			size_t readlen = fread(buf, 1, sizeof(buf), f);
			c->f = f;
			c->currentblockno = 1;
			c->actualblockno = 1;
			c->sendingfile = 1;
			
			struct { const packet_t *p; client_t *c; char *filename, *mode, *path; }
				ev = { p, c, filename, mode, tmp };
			CallEvent(EV_NEWWRITEREQUEST, &ev);
			
			SendData(c, buf, readlen);

#ifndef HAVE_STRNDUPA
			free(filename);
			free(mode);
#endif
			free(tmp);

			break;
                }
		default:
			dprintf("Got unknown packet: %d\n", ntohs(p->opcode));
			
			struct { const packet_t *p; client_t *c; } ev = { p, c };
			CallEvent(EV_UNKNOWN_PACKET, &ev);
			
			break;
	}
}
