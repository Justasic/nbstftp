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
#include "vec.h"

typedef struct listen_s
{
	char *bindaddr;
	short port;
} listen_t;

typedef struct conf_module_s
{
	char *name;
	char *path;
} conf_module_t;

typedef struct config_s
{
	char *directory;
	char *user;
	char *group;
	char *pidfile;
	char *modsearchpath;
	char daemonize;
	char fixpath;
	int readtimeout;
	vec_t(listen_t*) listenblocks;
	vec_t(conf_module_t*) moduleblocks;
} config_t;

// Defined in parser.y
extern config_t *config;

extern int ParseConfig(const char *filename);
extern void DeallocateConfig(config_t *conf);
