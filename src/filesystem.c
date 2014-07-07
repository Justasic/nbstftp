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
#include "filesystem.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int FileExists(const char *file)
{
	struct stat sb;

	if (stat(file, &sb) == -1)
		return 0;

	if (S_ISDIR(sb.st_mode))
		return 0;

	FILE *input = fopen(file, "r");

	if (input == NULL)
		return 0;
	else
		fclose(input);

	return 1;
}

int IsDirectory(const char *dir)
{
	struct stat fileinfo;
	
	if (stat(dir, &fileinfo) == 0)
	{
		if (S_ISDIR(fileinfo.st_mode))
			return 1;
	}
	
	return 0;
}
