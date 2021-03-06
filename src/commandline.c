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
#include "commandline.h"
#include "LICENSE.h"
#include "misc.h"
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include "sysconf.h"
#include <string.h>

static void PrintHelp(void)
{
	fprintf(stderr, "The no bullshit TFTP server, used to serve files over TFTP\n\n");
	fprintf(stderr, "Usage: nbstftp [OPTION]...\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "-f, --nofork        Do not daemonize (don't fork to background)\n");
	fprintf(stderr, "-c, --config        Specify a config file\n");
	fprintf(stderr, "-h, --help          This message\n");
	fprintf(stderr, "-l, --license       Print license message\n");
	fprintf(stderr, "-v, --version       Print version information\n");
	fprintf(stderr, "\nReport bugs to https://github.com/justasic/nbstftp/issues\n");
	
	exit(EXIT_FAILURE);
}

static void PrintLicense(void)
{
	// Print the license header. We have to allocate due to
	// no null terminator.
	char *str = nmalloc(LICENSE_len+1);
	memcpy(str, LICENSE, LICENSE_len);
	str[LICENSE_len+1] = 0;
	fprintf(stderr, "%s", str);
	free(str);
	exit(EXIT_FAILURE);
}

static void PrintVersion(void)
{
	fprintf(stderr, "The No Bullshit TFTP server version v" VERSION_FULL "\n\n");
	char *str = nmalloc(LICENSE_len+1);
	memcpy(str, LICENSE, LICENSE_len);
	str[LICENSE_len+1] = 0;
	fprintf(stderr, "%s", str);
	free(str);
	fprintf(stderr, "\n\nWritten by Justin Crawford\n");
	exit(EXIT_FAILURE);
}

void HandleArguments(int argc, char **argv)
{
	int wait_for_conf = 0, wait_for_port = 0;
	for (int i = 0; i < argc; ++i)
	{
		char *arg = argv[i];
		
		// Handle -- arguments.
		if (arg[0] == '-' && arg[1] == '-')
		{
			arg += 2;
			
			if (!strcasecmp(arg, "help"))
				PrintHelp();
			else if (!strcasecmp(arg, "license"))
				PrintLicense();
			else if (!strcasecmp(arg, "nofork"))
				nofork = 1;
			else if (!strcasecmp(arg, "version"))
				PrintVersion();
			else if (!strcasecmp(arg, "config"))
			{ // Make sure they actually specified a config file...
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a config file!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_conf = 1;
			}
			else
			{
				fprintf(stderr, "Unknown argument: \"%s\"\n", arg);
				PrintHelp();
			}
		}
		// Handle single dash arguments.
		else if (arg[0] == '-' && arg[1] != '-')
		{
			arg++;
			
			if (!strcasecmp(arg, "h"))
				PrintHelp();
			else if (!strcasecmp(arg, "l"))
				PrintLicense();
			else if (!strcasecmp(arg, "f"))
				nofork = 1;
			else if (!strcasecmp(arg, "v"))
				PrintVersion();
			else if (!strcasecmp(arg, "c"))
			{
				if (i+1 >= argc)
				{
					fprintf(stderr, "You must specify a config file!\n");
					exit(EXIT_FAILURE);
				}
				wait_for_conf = 1;
			}
			else
			{
				fprintf(stderr, "Unknown argument: \"%s\"\n", arg);
				PrintHelp();
			}
		}
		else // Handle other arguments.
		{
			// Copy the config file
			if (wait_for_conf)
				configfile = arg;
		}
	}
}
