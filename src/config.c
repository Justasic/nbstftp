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

#include "config.h"
#include "parser.h"
#include "misc.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern FILE *yyin;
extern int yyparse();
extern void yylex_destroy();
extern int lineno;

int ParseConfig(const char *filename)
{
	FILE *fd = fopen(filename, "r");
	if (!fd)
	{
		fprintf(stderr, "Failed to open config file %s: %s\n", filename, strerror(errno));
		return 1;
	}

	yyin = fd;

	int ret = yyparse();
	yylex_destroy();
	fclose(fd);

#ifndef NDEBUG
	printf("Config:\n");
	if (config)
	{
		printf(" Directory: %s\n User: %s\n Group: %s\n Daemonize: %d\n"
			" Pidfile: %s\n Read Timeout: %d\n",
			config->directory, config->user, config->group, config->daemonize, config->pidfile,
			config->readtimeout);
		
		listen_t *block;
		int i = 0;
		vec_foreach(&config->listenblocks, block, i)
		{
			printf("Listening On:\n Bind: %s\n Port: %d\n", block->bindaddr, block->port);
		}
		
	}
	else
		printf(" Config is null!\n");
#endif

	if (!config)
	{
		fprintf(stderr, "Failed to parse config file!\n");
		return 1;
	}

	if (!config->directory)
	{
		fprintf(stderr, "You must specify a directory to serve files from in the config!");
		return 1;
	}

	if (!config->pidfile)
		config->pidfile = "/var/run/nbstftp.pid";

	if (config->readtimeout < 2)
	{
		fprintf(stderr, "Error: Read Timeout time can be no less than 1 second! Setting to default of 5.\n");
		config->readtimeout = 5;
	}
	
	// It is stupid to do an access check here and should be done when
	// we're about to switch users (or just after)
	
	return ret;
}

void DeallocateConfig(config_t *conf)
{
	if (!config)
		return;

	// Clear memory.
	listen_t *block;
	int i = 0;
	vec_foreach(&config->listenblocks, block, i)
	{
		if (block->bindaddr)
			free(block->bindaddr);

		free(block);
	}
	
	vec_deinit(&config->listenblocks);
	
	conf_module_t *m;
	vec_foreach(&config->moduleblocks, m, i)
	{
		if (m->name)
			free(m->name);
		
		if (m->path)
			free(m->path);
	}
	
	vec_deinit(&config->moduleblocks);

	if (conf->user)
		free(conf->user);

	if (conf->group)
		free(conf->group);

	free(conf->directory);
	free(conf);
}

void yyerror(const char *s)
{
        fprintf(stderr, "Config: Error parsing line %d: %s\n", lineno+1, s);
        exit(EXIT_FAILURE);
}
