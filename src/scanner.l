%{
        #include "parser.h"
        int lineno = 0;
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
user          { return USER; }
group         { return GROUP; }
daemonize     { return DAEMONIZE; }


/* Ignore white space */
[ \t]             { }
\n                { ++lineno; }
[0-9]+            { yylval.ival = atoi(yytext); return CINT; }
\"[^\"\n]*[\"\n]  { yytext[yyleng - 1] = 0; yylval.sval = yytext + 1; return STR; }
"true|yes"        { yylval.bval = 1; }
"false|no"        { yylval.bval = 0; }
"/*"              { BEGIN(COMMENT); }
<COMMENT>"*/"     { BEGIN(INITIAL); }
<COMMENT>([^*]|\n)+|.
<COMMENT><<EOF>>  { printf("%d: Unterminated comment\n", lineno); return 0; }
"//".*\n          { /* Ignore comments */ }
.                 { return yytext[0]; }

%%

