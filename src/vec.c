/** 
 * Copyright (c) 2014 rxi
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
 * 
 * My copyright notice was kept for the minor modifications I had made to the libc version
 * to become compatible with this project. You may remove my copyright as long as you remove
 * my modifications.
 * 
 * Original Version found at: https://github.com/rxi/vec/tree/bd10a28c759a34212601efc5735fe082011a0092
 */

#include "vec.h"
#include <errno.h>
#include <assert.h>
#include "reallocarray.h"

#define VEC_MIN_CAPACITY 4


void vec_expand_(char **data, int *length, int *capacity, int memsz)
{
	assert(data && length && capacity && memsz);
	
	if (*length + 1 > *capacity)
	{
		if (*capacity == 0)
			*capacity = VEC_MIN_CAPACITY;
		else
			*capacity <<= 1;
	
		void *ndata = reallocarray(*data, *capacity, memsz);
		*data = ndata ? ndata : *data;
		
		if (!ndata)
			errno = ENOMEM;
	}
}


void vec_reserve_(char **data, int *length, int *capacity, int memsz, int n)
{
	assert(data && length && capacity && memsz);
	
	if (n > *capacity)
	{
		*capacity = n;
		void *ndata = reallocarray(*data, *capacity, memsz);
		*data = ndata ? ndata : *data;
		
		if (!ndata)
			errno = ENOMEM;
	}
}


void vec_compact_(char **data, int *length, int *capacity, int memsz)
{
	assert(data && length && capacity && memsz);
	
	if (*length == 0)
	{
		free(*data);
		*data = NULL;
		*capacity = 0;
		return;
	}
	
	*capacity = *length;
	void *ndata = reallocarray(*data, *capacity, memsz);
	*data = ndata ? ndata : *data;
	if (!ndata)
		errno = ENOMEM;
}


void vec_splice_(char **data, int *length, int *capacity, int memsz, int start, int count)
{
	assert(data && length && capacity && memsz && count);
	
	memmove(*data + start * memsz,
		*data + (start + count) * memsz,
		(*length - start - count) * memsz);
}


void vec_insert_(char **data, int *length, int *capacity, int memsz, int idx)
{
	assert(data && length && capacity && memsz);
	vec_expand_(data, length, capacity, memsz);
	// No reason to crash when trying to expand...
	if (errno == ENOMEM)
		return;
	
	memmove(*data + (idx + 1) * memsz,
		*data + idx * memsz,
		(*length - idx) * memsz);
}


void vec_swap_(char **data, int *length, int *capacity, int memsz, int idx1, int idx2)
{
	assert(data && length && capacity && memsz);
	char *tmp;
	vec_expand_(data, length, capacity, memsz);
	// No reason to crash when trying to expand...
	if (errno == ENOMEM)
		return;
	
	tmp = *data + *length * memsz;
	memcpy(tmp, *data + (idx1 * memsz), memsz);
	memcpy(*data + (idx1 * memsz), *data + (idx2 * memsz), memsz);
	memcpy(*data + (idx2 * memsz), tmp, memsz);
}
