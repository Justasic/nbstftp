/*
* Copyright (c) 2014-2015, Justin Crawford <Justasic@gmail.com>
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

// This macro is used just to increase readability,
// it is defined to duplicate a string out of a data
// block given to us without exceeding mlen.
// It will copy the string and then advance the data
// pointer by the size of the string it just copied
// plus the remaining null-terminating byte.
//
// This cannot be a function due to the fact that
// it still uses strndupa and it would be allocated
// on the called function's stack (eg, GetNext's stack space)
// and therefore it must be inside whatever function wants
// to copy the string (eg, ProcessPacket) as actual code.
// If this were C++ then we might be able to get away with it
// as a template but this isn't C++ so macro hackery it is!
#ifdef HAVE_STRNDUPA
	#define GetNext(str, data, mlen) \
	do { \
		str = strndupa(data, mlen); \
		data += strnlen(str, mlen) + 1; \
	} while(0)
#else
	#define GetNext(str, data, mlen) \
	do { \
		str = strndup(data, mlen); \
		data += strnlen(str, mlen) + 1; \
		} while(0)
#endif

// Process the incoming packet.
void ProcessPacket(client_t *c, const packet_t * const p, size_t len, size_t alloclen)
{
	// Sanity check
	if (len > MAX_PACKET_SIZE)
	{
		printf("Received an invalidly sized packet.\n");
		return;
	}

	assert(p);

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

			bprintf("Got a data packet\n");

			struct { const packet_t * const p; client_t *c; } ev = { p, c };
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
					   flen, SizeReduce(c->bytestransferred));
				free(tmp2);

				Acknowledge(c, ntohs(p->blockno));

				if ((len - sizeof(packet_t)) < 512)
				{
					printf("Got end of data packet, %s transferred in %zu blocks\n",
						   SizeReduce(c->bytestransferred), c->actualblockno);
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

			printf("Aborting file transfer.\n");
			// Close the file and clean up.
			c->destroy = 1;

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
			printf("Got Acknowledgement packet for block %d%s from %s (%s transferred)\n",
			       ntohs(p->blockno), ntohs(p->blockno) == c->actualblockno ? "" : tmp2,
				   GetAddress(c->s.addr), SizeReduce(c->bytestransferred));
			free(tmp2);

			struct { const packet_t * const p; client_t *c; } ev = { p, c };
			CallEvent(EV_ACK_PACKET, &ev);

			if (c->sendingfile)
			{
				if (!c->blk)
					c->blk = nmalloc(c->blksize);
				memset(c->blk, 0, c->blksize);
				size_t readlen = fread(c->blk, 1, c->blksize, c->f);

				printf("Read %zu bytes from file\n", readlen);

				c->currentblockno++;
				c->actualblockno++;

				// Sending a file
				SendData(c, c->blk, readlen);

				// We're at the end of the file.
				if (MIN(c->blksize, readlen) != c->blksize)
				{
					printf("Finished sending file, %s transferred in %zu %d-sized blocks\n",
						   SizeReduce(c->bytestransferred), c->actualblockno, c->blksize);
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
			// Get the filename and modes
			//
			// Since we dig only past the first value in the struct, we only
			// get the size of that first value (eg, the uint16_t)
			// Use strndupa to use the stack frame for temporary allocation with a
			// max length of 512 bytes. This will prevent buffer-overflow exploits (or so I hope)
			size_t maxlen = alloclen - sizeof(uint16_t);
			// Offset the packet pointer by the size of the TFTP header.
			const char *data = ((const char *)p) + sizeof(uint16_t);
			// Define all the things we must check for in this packet.
			char *filename, *mode, *opt, *optparam;
			// Get the filename
			GetNext(filename, data, maxlen);
			// Get the mode of the file transfer (eg, netascii, octet, or mail)
			GetNext(mode, data, maxlen);
			// As per RFC2347, RFC2348, and RFC2349 we have an option parameter given
			// We must copy it to see what the option is, there is an additional option
			// parameter below.
			GetNext(opt, data, maxlen);
			if (strnlen(opt, maxlen) != 0)
				GetNext(optparam, data, maxlen);


			printf("Got write request packet for file \"%s\" in mode %s with option \"%s\" param \"%s\"\n", filename, mode, opt, optparam);

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

			bprintf("Checking we can open file \"%s\"\n", tmp);

			// Check that we have access to that file
			if (access(tmp, W_OK) == -1 || access(config->directory, W_OK) == -1)
			{
				if (errno == EACCES)
				{
					Error(c, ERROR_NOUSER, "Cannot access file: %s", strerror(errno));
					goto end;
				}
			}

			bprintf("Opening file %s as %s\n", tmp, imode == 0 ? "wt" : "wb");
			FILE *f = fopen(tmp, imode == 0 ? "wt" : "wb");
			if (!f)
			{
				Error(c, ERROR_NOFILE, "Cannot write file: %s", strerror(errno));
				goto end;
			}

			bprintf("File %s is available for write, writing first packet...\n", tmp);

			c->f = f;
			c->currentblockno = 1;
			c->actualblockno = 1;
			c->sendingfile = 1;

			struct { const packet_t * const p; client_t *c; char *filename, *mode, *path; }
				ev = { p, c, filename, mode, tmp };
			CallEvent(EV_NEWWRITEREQUEST, &ev);

			// Acknowledge our transfer request
			Acknowledge(c, 0);

end:
#ifndef HAVE_STRNDUPA
			free(filename);
			free(mode);
			free(opt);
			free(optparam);
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
			// Get the filename and modes
			//
			// Since we dig only past the first value in the struct, we only
			// get the size of that first value (eg, the uint16_t)
			// Use strndupa to use the stack frame for temporary allocation with a
			// max length of 512 bytes. This will prevent buffer-overflow exploits (or so I hope)
			size_t maxlen = alloclen - sizeof(uint16_t);
			// Offset the packet pointer by the size of the TFTP header.
			const char *data = ((const char *)p) + sizeof(uint16_t);
			// Define all the things we must check for in this packet.
			char *filename, *mode, *opt, *optparam;
			// Get the filename
			GetNext(filename, data, maxlen);
			// Get the mode of the file transfer (eg, netascii, octet, or mail)
			GetNext(mode, data, maxlen);
			// As per RFC2347, RFC2348, and RFC2349 we have an option parameter given
			// We must copy it to see what the option is, there is an additional option
			// parameter below.
			GetNext(opt, data, maxlen);
			if (strnlen(opt, maxlen) != 0)
				GetNext(optparam, data, maxlen);

			// mode can be "netascii", "octet", or "mail" case insensitive.
			printf("Got read request packet: \"%s\" -> \"%s\" with option \"%s\" param \"%s\"\n", filename, mode, opt, optparam);

			// We don't support mail-mode
			if (!strcasecmp(mode, "mail"))
			{
				Error(c, ERROR_ILLEGAL, "Mail mode not supported by NBSTFTP");
				break;
			}

			int imode = strcasecmp(mode, "netascii"), tsize = 0;

			// Get the blocksize
			if (!strcasecmp(opt, "blksize"))
			{
				errno = 0;
				long blksize = strtol(optparam, NULL, 10);
				// the block size is not in range.
				if (errno == ERANGE)
				{
					Error(c, ERROR_OPTION, "Invalid block size %s: %s (%d)", optparam, strerror(errno), errno);
					break;
				}

				// Make sure the block size is acceptable
				if (blksize < 8 || blksize > 65464)
				{
					Error(c, ERROR_OPTION, "Invalid block size %ld", blksize);
					break;
				}
				// Okay. it's a valid block size, reply with an OptionAcknowledgement
				// then start servicing the request after the next ack packet
				printf("Servicing block size request of %ld\n", blksize);
				OptionAcknowledge(c, opt, optparam);
				// Mark that we're already sending the file, this will be used in the
				// logic below, making the ACK reply the first packet sent.
				c->sendingfile = 1;
				c->currentblockno = 0;
				c->actualblockno = 0;
				// Set the block size to send.
				c->blksize = blksize;
			}
			else if (!strcasecmp(opt, "tsize"))
			{
				tsize = 1;
				// TODO. do nothing for now
			}
			else if (!strcasecmp(opt, "timeout"))
			{
				// TODO. do nothing for now.
			}

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

			// This is up her because we declare a variable inside
			// a critical section between the goto jump and the compiler will whine
			// even though it is only used within that critical section.
			struct { const packet_t * const p; client_t *c; char *filename, *mode, *path; }
			ev = { p, c, filename, mode, tmp };
			uint8_t buf[512];

			if (tsize)
			{
				bprintf("Client wants to know size of file \"%s\" (which is %s), responding...\n", tmp, SizeReduce(len));
				free(tmp);
				tmp = NULL;
				asprintf(&tmp, "%zu", len);
				OptionAcknowledge(c, opt, tmp);
				fclose(f);
				goto skipfilesend;
			}

			bprintf("File \"%s\" is %s long, sending first packet\n", tmp, SizeReduce(len));

			// file buffer
			c->f = f;


			if (c->sendingfile)
				goto skipfilesend;
			else
			{
				c->sendingfile = 1;
				c->currentblockno = 1;
				c->actualblockno = 1;
			}

			memset(buf, 0, sizeof(buf));
			size_t readlen = fread(buf, 1, sizeof(buf), f);

			CallEvent(EV_NEWWRITEREQUEST, &ev);
			SendData(c, buf, readlen);
skipfilesend:

#ifndef HAVE_STRNDUPA
			free(filename);
			free(mode);
			free(opt);
			free(optparam);
#endif
			free(tmp);

			break;
		}
		default:
			bprintf("Got unknown packet: %d\n", ntohs(p->opcode));

			struct { const packet_t * const p; client_t *c; } ev = { p, c };
			CallEvent(EV_UNKNOWN_PACKET, &ev);

			break;
	}
}
