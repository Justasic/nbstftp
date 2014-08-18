%{
        #include "parser.h"
        int lineno = 0;
        int commentstart = 0;
%}


%option noyywrap
%option nounput
%option noinput

%x COMMENT

%%

 /* the tokens */
server        { return SERVER; }
bind          { return BIND; }
port          { return PORT; }
directory     { return DIRECTORY; }
fixpath       { return FIXPATH; }
user          { return USER; }
group         { return GROUP; }
pidfile       { return PIDFILE; }
daemonize     { return DAEMONIZE; }
readtimeout   { return READTIMEOUT; }
listen        { return LISTEN; }
module        { return MODULE; }
name          { return NAME; }
path          { return PATH; }
modulesearchpath { return MODSEARCHPATH; }

 /* Ignore white space */
[ \t]                 { }
\n                    { ++lineno; }
[0-9]+                { yylval.ival = atoi(yytext); return CINT; }
\"[^\"\n]*[\"\n]      { yytext[yyleng - 1] = 0; yylval.sval = yytext + 1; return STR; }
true|yes              { yylval.bval = 1; return BOOL; }
false|no              { yylval.bval = 0; return BOOL; }
"/*"                  { BEGIN(COMMENT); commentstart = lineno; }
<COMMENT>"*/"         { BEGIN(INITIAL); }
<COMMENT>([^*]|\n)+|. { ++lineno; }
<COMMENT><<EOF>>      { printf("line %d: Unterminated comment\n", commentstart+1); return 0; }
"//".*\n              { /* Ignore comments */ ++lineno; }
.                     { return yytext[0]; }

%%

