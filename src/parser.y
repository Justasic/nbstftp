%{
#include "config.h"
#include "misc.h"
#include "module.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int yylex();
extern void yyerror(const char *s);

config_t *config;
listen_t *curblock;
conf_module_t *curmod;
%}

%error-verbose

%union
{
        int ival;
        char *sval;
        char bval;
}

%token <ival> CINT
%token <sval> STR
%token <bval> BOOL

%token SERVER
%token BIND
%token PORT
%token DIRECTORY
%token USER
%token GROUP
%token PIDFILE
%token DAEMONIZE
%token READTIMEOUT
%token LISTEN
%token FIXPATH
%token MODULE
%token NAME
%token PATH
%token MODSEARCHPATH

%%

conf: | conf conf_items;

conf_items: server_entry | listen_entry | module_entry;

module_entry: MODULE
{
	conf_module_t *m = nmalloc(sizeof(conf_module_t));
	m->path = NULL;
	m->name = NULL;
	curmod = m;
	
	if (!config)
	{
		config = nmalloc(sizeof(config_t));
		config->daemonize = -1;
		config->readtimeout = 5;
		config->fixpath = 1;
		vec_init(&config->listenblocks);
		vec_init(&config->moduleblocks);
	}
	
	vec_push(&config->moduleblocks, m);
}
'{' module_items '}';

listen_entry: LISTEN
{
	listen_t *block = nmalloc(sizeof(listen_t));
	block->port = -1;
	block->bindaddr = NULL;
	
	curblock = block;
	
	if (!config)
	{
		config = nmalloc(sizeof(config_t));
		config->daemonize = -1;
		config->readtimeout = 5;
		config->fixpath = 1;
		vec_init(&config->listenblocks);
		vec_init(&config->moduleblocks);
	}
	
	vec_push(&config->listenblocks, block);
}
'{' listen_items '}';

server_entry: SERVER
{
	config = nmalloc(sizeof(config_t));
	// Defaults
	config->daemonize = 1;
	config->readtimeout = 5;
	config->fixpath = 1;
	vec_init(&config->listenblocks);
	vec_init(&config->moduleblocks);
}
'{' server_items '}';

server_items: | server_item server_items;
server_item: server_directory | server_user | server_group | server_daemonize | server_pidfile | server_readtimeout | server_fixpath
| server_module_search_path;

listen_items: | listen_item listen_items;
listen_item: listen_bind | listen_port;

module_items: | module_item module_items;
module_item: module_path | module_name;

module_path: PATH '=' STR ';'
{
	curmod->path = strdup(yylval.sval);
};

module_name: NAME '=' STR ';'
{
	curmod->name = strdup(yylval.sval);
};

listen_bind: BIND '=' STR ';'
{
	curblock->bindaddr = strdup(yylval.sval);
	if (!curblock->bindaddr)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

listen_port: PORT '=' CINT ';'
{
	curblock->port = yylval.ival;
};

server_directory: DIRECTORY '=' STR ';'
{
	config->directory = strdup(yylval.sval);
	if (!config->directory)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

server_user: USER '=' STR ';'
{
	config->user = strdup(yylval.sval);
	if (!config->user)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

server_group: GROUP '=' STR ';'
{
	config->group = strdup(yylval.sval);
	if (!config->group)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

server_module_search_path: MODSEARCHPATH '=' STR ';'
{
	config->modsearchpath = strdup(yylval.sval);
};

server_daemonize: DAEMONIZE '=' BOOL ';'
{
	config->daemonize = yylval.bval;
};

server_pidfile: PIDFILE '=' STR ';'
{
	config->pidfile = strdup(yylval.sval);
	if (!config->pidfile)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

server_readtimeout: READTIMEOUT '=' CINT ';'
{
	config->readtimeout = yylval.ival;
};

server_fixpath: FIXPATH '=' BOOL ';'
{
	config->fixpath = yylval.bval;
};
