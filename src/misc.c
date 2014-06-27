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
#define _POSIX_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>    // For /etc/passwd related functions
#include <grp.h>    // For /etc/group related functions
#include <unistd.h>
#include <sys/types.h>

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
	// Allocate the pointer, allocate 1 byte if there was an integer overflow.
	void *ptr = malloc(sz ? sz : 1);
	
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

// OpenBSD-like saferealloc.
#ifndef __OpenBSD__
void *saferealloc(void *ptr, size_t length)
{
	void *newptr;
	
	if(length <= 0)
		length = 1;
	
	/* If ptr is NULL then realloc does a malloc */
	newptr = realloc(ptr, length);
	if (newptr == 0)
	{
		fprintf(stderr, "realloc failed to set pointer %p to size %lu: %s",
			ptr, length, strerror(errno));
		exit(EXIT_FAILURE);
	}
	
	return newptr;
}
#endif

// Change userid and groupid to this user.
int SwitchUserAndGroup(const char *user, const char *group)
{
	// It is not a failure to have both user and group null.
	// It simply means do nothing.
	if (!user && !group)
		return 0;
	
	if (user)
	{
		errno = 0;
		struct passwd *pass = getpwnam(user);
		if (!pass)
		{
			fprintf(stderr, "Cannot getpwnam %s: %s\n", user, strerror(errno));
			return 1;
		}
		
		if (setuid(pass->pw_uid) == -1)
		{
			fprintf(stderr, "Cannot setuid to %s: %s\n", user, strerror(errno));
			return 1;
		}
		
		// Save on one syscall at least.
		if (group && !strcmp(user, group))
		{
			errno = 0;
			if (setgid(pass->pw_gid) == -1)
			{
				fprintf(stderr, "Cannot setgid to %s: %s\n", group, strerror(errno));
				return 1;
			}
			return 0;
		}
	}
	
	if (group)
	{
		errno = 0;
		struct group *grp = getgrnam(group);
		if (!grp)
		{
			fprintf(stderr, "Cannot getgrnam %s: %s\n", group, strerror(errno));
			return 1;
		}
		
		if (setgid(grp->gr_gid) == -1)
		{
			fprintf(stderr, "Cannot setgid to %s: %s\n", group, strerror(errno));
			return 1;
		}
	}
	
	return 0;
}

int InTerm(void)
{
	return isatty(fileno(stdout) && isatty(fileno(stdin)) && isatty(fileno(stderr)));
}

// Daemonize (aka. Fork)
void Daemonize(void)
{
	extern int nofork;
	if (!nofork && InTerm())
	{
		int i = fork();
		
		// Exit our parent process.
		if (i > 0)
			exit(EXIT_SUCCESS);
		
		// Say our PID to the console.
		printf("Going away from console, pid: %d\n", getpid());
		
		// Close all the file descriptors so printf and shit goes
		// away. This can later be used for logging instead.
		freopen("/dev/null", "r", stdin);
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		
		if (setpgid(0, 0) < 0)
			die("Unable to setpgid()");
		else if (i == -1)
			die("Unable to daemonize");
	}
}
