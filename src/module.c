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

#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <assert.h>

#include "module.h"
#include "config.h"
#include "vec.h"

vec_t(module_t) modules;

// Load a module from filesystem.
int LoadModule(module_t *m, const char *str)
{
	assert(m && str);
	dlerror();
	
	// First, fill out the struct
	m->handle = dlopen(str, RTLD_LAZY);
	
	if (!m->handle)
	{
		fprintf(stderr, "Failed to load module \"%s\": %s\n", str, dlerror());
		return -1;
	}
	m->path = strdup(str);
	
	// Now get the module info struct from the module itself.
	m->minfo = (module_info_t*) dlsym(m->handle, "_minfo");
	// Some platforms prefix symbols with an additional underscore, try to find this one.
	if (!m->minfo)
	{
		m->minfo = (module_info_t*) dlsym(m->handle, "__minfo");
		if (!m->minfo)
		{
			fprintf(stderr, "Failed to find module information structure for module \"%s\": %s\n", str, dlerror());
			dlclose(m->handle);
			return -1;
		}
	}
	
	vec_push(&modules, *m);
	
	// Call our load event.
	CallEvent(EV_MODLOAD, m);
	
	return 0;
}

// Unload a module
void UnloadModule(module_t m)
{
	module_t mod;
	int idx;
	
	CallEvent(EV_MODUNLOAD, &m);
	
	vec_foreach(&modules, mod, idx)
	{
		if (!strcmp(m.minfo->name, mod.minfo->name))
		{
			vec_splice(&modules, idx, 1);
			break;
		}
	}
	
	if (m.handle)
		dlclose(m.handle);
	
	if (m.path)
		free(m.path);
}

// Find a module
int FindModule(module_t *mod, const char *name)
{
	module_t m;
	int idx;
	
	vec_foreach(&modules, m, idx)
	{
		if (!strcmp(m.minfo->name, name))
		{
			memcpy(mod, &m, sizeof(module_t));
			return 0;
		}
	}
	
	return -1;
}

void InitializeModules(void)
{
	// do something? We have a config...
	
	// Initialize the modules list.
	vec_init(&modules);
	
	conf_module_t *cm;
	int idx;
	vec_foreach(&config->moduleblocks, cm, idx)
	{
		module_t m;
		// TODO: Load from module path + module name.
		if (cm->path)
			LoadModule(&m, cm->path);
		else
			LoadModule(&m, cm->name);
	}
}

// Call a specific module's event.
void CallModuleEvent(module_t m, int type, void *data)
{
	event_t ev;
	int idx;
	
	vec_foreach(&m.minfo->events, ev, idx)
	{
		if (ev.type == type)
			ev.func(data);
	}
}

// Globally call an event (such as receiving a packet)
void CallEvent(int type, void *data)
{
	module_t m;
	int idx;
	
	// Call each module's event for this particular evet.
	vec_foreach(&modules, m, idx)
	{
		CallModuleEvent(m, type, data);
	}
}