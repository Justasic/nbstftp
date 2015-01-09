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
#include "filesystem.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>    // For /etc/passwd related functions
#include <grp.h>    // For /etc/group related functions

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
	
	// Do a final access check to make sure we can read it
	// otherwise it's pointless to report a file we can't read.
	if (access(file, R_OK) == -1)
		return 0;

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

int SetFilePermissions(const char *file, const char *user, const char *group, mode_t permissions)
{
	assert(file);
	
	if (!user && !group)
		return 0;
	
	uid_t uid = 0;
	gid_t gid = 0;
	
	if (user)
	{
		errno = 0;
		struct passwd *pass = getpwnam(user);
		if (!pass)
		{
			fprintf(stderr, "Cannot getpwnam %s: %s\n", user, strerror(errno));
			return 1;
		}
		uid = pass->pw_uid;
		
		// Save on one syscall at least.
		if (group && !strcmp(user, group))
		{
			gid = pass->pw_gid;
			goto perms;
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
		
		gid = grp->gr_gid;
	}
	
perms:
	if (chown(file, uid, gid) == -1)
	{
		fprintf(stderr, "Failed to set owner on file %s to %s %s: %s\n", file, user, group, strerror(errno));
		return 1;
	}
	
	if (chmod(file, permissions) == -1)
	{
		fprintf(stderr, "Failed to set permissions on file %s to %.4d\n", file, permissions);
		return 1;
	}
	
	return 0;
}

// This is used to convert Windows-style path strings to linux/unix.
// Windows supports unix-like paths but Linux does not support Windows-like
// path strings. So netbooting windows (which requires windows-style path strings)
// fails because Linux returns File Not Found when accessing the file path.
void FixPath(char *str)
{
	size_t len = strlen(str);
	while (len--)
	{
		if (str[len] == '\\')
			str[len] = '/';
	}
}