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

#include "signalhandler.h"
#include "config.h"
#include "module.h"
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

extern char *configfile;
extern int running;

static void SignalHandler(int sig)
{
	switch (sig)
	{
		case SIGHUP: // Rehash
		{
// 			signal(sig, SIG_IGN);
			printf("Rehashing configuration...\n");
			config_t *oldconf = config;
			if (ParseConfig(configfile) == 1)
				printf("Rehash failed.\n");
			else
			{
				printf("Rehash successful.\n");
				DeallocateConfig(oldconf);
			}
			break;
		}
		case SIGSEGV:
                {
			static int cnt = 0;
			// Double segfault
			if (cnt++ >= 1)
				exit(1);

			fprintf(stderr, "FATAL: well.. shit. We have a Segmentation Fault. Best whip out the debugger...\n");
			break;
                }
		case SIGTERM:
		case SIGINT:
// 			signal(sig, SIG_IGN);
			printf("Received quit signal, quitting...\n");
			running = 0;
			break;
		case SIGPIPE:
			printf("Received SIGPIPE, ignoring...\n");
			break;
		default:
			printf("Received unknown signal %d\n", sig);
			break;
	}
	
	// Inform our modules about it.
	CallEvent(EV_SIGNAL, &sig);
}

void RegisterSignalHandlers(void)
{
	signal(SIGSEGV, SignalHandler);
	signal(SIGINT, SignalHandler);
	signal(SIGTERM, SignalHandler);
	signal(SIGHUP, SignalHandler);
	signal(SIGPIPE, SignalHandler);
}
