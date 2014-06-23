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

#include "misc.h"
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

void die(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	
	size_t len = strlen(msg) * 2;
	char *str = malloc(len);
	if (!str)
	{
		// Boy.. this would be embarrassing...
		fprintf(stderr, "FATAL ERROR: Cannot format previous error message and terminate: %s\n", strerror(errno));
		fprintf(stderr, "Previous unformatted error message was: \"%s\"\n", msg);
		exit(EXIT_FAILURE);
	}
	
	int newlen = vsnprintf(str, len, msg, ap);
	va_end(ap);
	
	fprintf(stderr, "FATAL: %s\n", str);
	free(str);
	
	exit(EXIT_FAILURE);
}

// Our own error checking and nullification malloc.
// the __attribute__ is to tell gcc that this function is malloc-like
// so it can optimize calls to this function (which should always return
// a pointer. if an error condition happens, it terminates here and now.)
__attribute__((malloc))
void *nmalloc(size_t sz)
{
	// Allocate the pointer
	void *ptr = malloc(sz);
	
	// Make sure no errors happened.
	if (!ptr)
	{
		fprintf(stderr, "FATAL: Cannot allocate memory: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	// ensure the memory space is null.
	memset(ptr, 0, sz);
	
	// bye!
	return ptr;
}
