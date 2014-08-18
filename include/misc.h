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
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include "reallocarray.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

#ifndef NDEBUG
# define dprintf(str, ...) printf(str, __VA_ARGS__)
# define dfprintf(thing, str, ...) fprintf(thing, str, __VA_ARGS__)
#else
# define dprintf(str, ...)
# define dfprintf(thing, str, ...)
#endif

extern int running;

// Allocators
extern __attribute__((malloc)) void *nmalloc(size_t sz);
// The function never returns since it's a fatal error function.
// tell the compiler that this kills the program so it can optimize
// for it.
extern void die(const char *, ...) __attribute__ ((noreturn));
extern int SwitchUserAndGroup(const char *user, const char *group);
extern void Daemonize(void);
extern int vasprintf(char **str, const char *fmt, va_list args);
extern char *SizeReduce(size_t size);
extern char *GetBlockSize(size_t blocks);
extern char *stringify(const char *str, ...);
