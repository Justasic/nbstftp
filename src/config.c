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

#include "config.h"
#include "parser.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

extern FILE *yyin;
extern int yyparse();
extern void yylex_destroy();
extern int lineno;

int ParseConfig(const char *filename)
{
        FILE *fd = fopen(filename, "r");
        if (!fd)
        {
                fprintf(stderr, "Failed to open config file: %s\n", filename);
                exit(EXIT_FAILURE);
        }

        yyin = fd;

        int ret = yyparse();
        yylex_destroy();
	fclose(fd);
	
	
	printf("Config:\n");
	if (config)
		printf(" Bind: %s\n Port: %d\n Directory: %s\n User: %s\n Group: %s\n Daemonize: %d\n",
		       config->bindaddr, config->port, config->directory, config->user, config->group,
	 config->daemonize);
	else
		printf("Config is null!\n");
	
	if (!config)
	{
		fprintf(stderr, "Failed to parse config file!\n");
		exit(EXIT_FAILURE);
	}
	
	if (!config->directory)
	{
		fprintf(stderr, "You must specify a directory to serve files from in the config!\n");
		exit(EXIT_FAILURE);
	}
	
	// Default is to listen on all interfaces
	if (!config->bindaddr)
		config->bindaddr = "0.0.0.0";
	
	// Default is to use port 69
	if (config->port == -1)
		config->port = 69;
	
	if (!config->pidfile)
		config->pidfile = "/var/run/nbstftp.pid";
	
	
        return ret;
}

void DeallocateConfig(config_t *conf)
{
	// Clear memory.
	if (conf->bindaddr)
		free(conf->bindaddr);
	
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
