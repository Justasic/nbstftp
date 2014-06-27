%{
#include "config.h"
#include "misc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern int yylex();
extern void yyerror(const char *s);

config_t *config;
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


%%

conf: | conf conf_items;

conf_items: server_entry;

server_entry: SERVER
{
	config = nmalloc(sizeof(config_t));
	// Defaults
	config->port = -1;
	config->daemonize = 1;
	config->readtimeout = 5;
}
'{' server_items '}';

server_items: | server_item server_items;
server_item: server_bind | server_port | server_directory | server_user | server_group | server_daemonize |
server_pidfile | server_readtimeout;


server_bind: BIND '=' STR ';'
{
	config->bindaddr = strdup(yylval.sval);
	if (!config->bindaddr)
	{
		fprintf(stderr, "Failed to parse config: %s\n", strerror(errno));
		exit(1);
	}
};

server_port: PORT '=' CINT ';'
{
	config->port = yylval.ival;
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
