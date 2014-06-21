%{
#include "config.h"
#include <stdlib.h>
#include <string.h>

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


%%

conf: | conf conf_items;

conf_items: server_entry;

server_entry: SERVER
{
	config = malloc(sizeof(config_t));
	memset(config, 0, sizeof(config_t));
	// Defaults
	config->port = -1;
	config->daemonize = 1;
}
'{' server_items '}';

server_items: | server_item server_items;
server_item: server_bind | server_port | server_directory | server_user | server_group | server_daemonize | server_pidfile;


server_bind: BIND '=' STR ';'
{
	config->bindaddr = strdup(yylval.sval);
};

server_port: PORT '=' CINT ';'
{
	config->port = yylval.ival;
};

server_directory: DIRECTORY '=' STR ';'
{
	config->directory = strdup(yylval.sval);
};

server_user: USER '=' STR ';'
{
	config->user = strdup(yylval.sval);
};

server_group: GROUP '=' STR ';'
{
	config->group = strdup(yylval.sval);
};


server_daemonize: DAEMONIZE '=' BOOL ';'
{
	config->daemonize = yylval.bval;
};

server_pidfile: PIDFILE '=' STR ';'
{
	config->pidfile = strdup(yylval.sval);
};
