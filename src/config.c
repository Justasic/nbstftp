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
        fclose(fd);
        yylex_destroy();
        return ret;
}

void yyerror(const char *s)
{
        fprintf(stderr, "Config: Error parsing line %d: %s\n", lineno, s);
        exit(EXIT_FAILURE);
}
