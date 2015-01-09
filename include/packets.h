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
#pragma once
#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>

#ifdef __FreeBSD__
# include <sys/socket.h>
# include <netinet/in.h>
#endif

enum
{
	PACKET_RRQ = 1, // Read Request packet
	PACKET_WRQ,     // Write Request packet
	PACKET_DATA,    // Data packet
	PACKET_ACK,     // Acknowledgement packet
	PACKET_ERROR    // Error packet
};

/*
 * Error Codes
 *
 * Value     Meaning
 * 0         Not defined, see error message (if any).
 * 1         File not found.
 * 2         Access violation.
 * 3         Disk full or allocation exceeded.
 * 4         Illegal TFTP operation.
 * 5         Unknown transfer ID.
 * 6         File already exists.
 * 7         No such user.
 */

enum
{
	ERROR_UNDEFINED,
	ERROR_NOFILE,
	ERROR_ACCESS,
	ERROR_DISKFULL,
	ERROR_ILLEGAL,
	ERROR_UNKNOWNTID,
	ERROR_FILEEXISTS,
	ERROR_NOUSER
};

// Max size of a TFTP packet, this size may change in the future
// I gather that it should be around 512 bytes big, not 1024.
#define MAX_PACKET_SIZE 516


// Define our packet structure for the DATA, ERROR, and ACK packets
typedef struct packet_s
{
	uint16_t opcode;
	uint16_t blockno; // Also errnum for ERROR packet
	// void *data;
} packet_t;

// Handle client information
typedef union socketstructs_s
{
	struct sockaddr_in in;
	struct sockaddr_in6 in6;
	struct sockaddr sa;
} socketstructs_t;

// Forward declare to prevent recursive includes.
typedef struct client_s client_t;

// Functions
__attribute__((format(printf, 3, 4)))
extern void Error(client_t *client, const uint16_t errnum, const char *str, ...);
extern void Acknowledge(client_t *client, uint16_t blockno);
extern void SendData(client_t *client, void *data, size_t len);
