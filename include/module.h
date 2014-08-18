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
#include "vec.h"

enum
{
	EV_BEGIN,
		EV_MODLOAD,           // Module was loaded.
		EV_MODUNLOAD,         // Module was unloaded.
		EV_SOCKETACTIVITY,    // Socket has a read or write.
		EV_NEWREADREQUEST,    // New file read request.
		EV_NEWWRITEREQUEST,   // New file write request.
		EV_DATA_PACKET,       // New data packet.
		EV_ACK_PACKET,        // New acknowledgement packet.
		EV_ERROR_PACKET,      // New error packet.
		EV_UNKNOWN_PACKET,    // An unknown packet.
		EV_SENDING_PACKETS,   // We're sending packets out.
		EV_RECEIVING_PACKETS, // We're receiving packets.
		EV_RESEND,            // We're resending a packet which failed to resend.
		EV_TICK,              // Each time the event loop iterates, we call this event.
		EV_SIGNAL,            // We've received a signal from the kernel.
	EV_END
};

typedef struct event_s 
{
	int type;
	void (*func)(void *data);
} event_t;

// Module functions and structs.

// This struct is delcared in each module as _minfo
// and specifies what hook to call for this module.
typedef struct module_info_s
{
	const char *name;
	const char *author;
	const char *version;
	// Events, functions, etc.?
	
	// Module events and functions
	vec_t(event_t) events;
} module_info_t;

typedef struct module_s
{
	// Module handle, path, and info from the module.
	void *handle;
	char *path;
	module_info_t *minfo;
} module_t;

extern int LoadModule(module_t *m, const char *str);
extern void UnloadModule(module_t m);
extern int FindModule(module_t *mod, const char *name);
extern void InitializeModules(void);

// Event related functions
extern void CallModuleEvent(module_t m, int type, void *data);
extern void CallEvent(int type, void *data);


// Our declare module function which gives header info and such.
#define DECLARE_MODULE(name, author, version) \
	module_info_t _minfo = { name, author, version }