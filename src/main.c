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
#include <unistd.h>
#include <signal.h>

#include "config.h"
#include "commandline.h"
#include "filesystem.h"
#include "client.h"
#include "misc.h"
#include "signalhandler.h"
#include "socket.h"
#include "sysconf.h"
#include "module.h"
//#include "packets.h"

int running = 1;
// Fork to background unless otherwise specified
int nofork = -1;
char *configfile = NULL;

void WritePID(void)
{
	FILE *f = NULL;
	// Make sure the process doesn't already exist.
	if (FileExists(config->pidfile))
	{
		f = fopen(config->pidfile, "r");
		if (!f)
		{
			// wtf? the pid file exists but it doesnt?
			// whatever. just write our own pid file.
			goto write;
		}
		
		// Read the PID file, 
		char pidstr[1024];
		memset(pidstr, 0, sizeof(pidstr));
		fread(pidstr, 1, sizeof(pidstr), f);
		fclose(f);
		
		// Convert from string to int.
		pid_t pid = atoi(pidstr);
		
		// Check if the process is running.
		if (kill(pid, 0) == 0)
			die("A nbstftp daemon is already running!");
		else
		{
			printf("Removing stale pid file %s\n", config->pidfile);
			unlink(config->pidfile);
		}
	}
write:
	f = fopen(config->pidfile, "w");
	if (!f)
	{
		fprintf(stderr, "WARNING: Failed to open PID file for write (File: %s)\n", config->pidfile);
		return;
	}
	
	fprintf(f, "%d", getpid());
	fclose(f);
	
	// If we're part of a user or group, change to that user/group so
	// we can remove it when we shut down.
	if (config->user || config->group)
		SetFilePermissions(config->pidfile, config->user, config->group, 0777);
}

int main(int argc, char **argv)
{
	HandleArguments(argc, argv);
	
	RegisterSignalHandlers();

	if (!configfile)
		configfile = CMAKE_INSTALL_PREFIX "/etc/nbstftp.conf";

	if (ParseConfig(configfile) != 0)
		die("Failed to parse the config file!");
	
	// Write the PID file -- Also check for any other
	// running versions of us.
	WritePID();
	
	// Initialize the client pool
	vec_init(&clientpool);
	
	// Initialize our modules
	InitializeModules();
	
	// Initialize the socket system.
	if (InitializeSockets() == -1)
		die("Failed to initialize and bind to the interfaces!");

	// Change the user and group id.
	if (SwitchUserAndGroup(config->user, config->group) == 1)
	{
		fprintf(stderr, "Failed to set user id or group id!\n");
		goto cleanup;
	}
	
	if (nofork == -1)
		nofork = !config->daemonize;
	
	// Go away.
	Daemonize();
	
	// Enter idle loop.
	while (running)
	{
		// Check the clients
		CheckClients();
		
		// Process packets or wait on the sockets.
		ProcessSockets();
		
		// Tick modules
		CallEvent(EV_TICK, NULL);
	}

cleanup:
	
	// Close the file descriptors.
	ShutdownSockets();
	
	// Deallocate client pool
	DeallocateClients();
	
	// Remove our PID
	if (config)
		unlink(config->pidfile);

	// Cleanup memory
	DeallocateConfig(config);
	
	return EXIT_SUCCESS;
}
