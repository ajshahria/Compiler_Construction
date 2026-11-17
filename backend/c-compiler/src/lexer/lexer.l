%option noyywrap yylineno nounput noinput
%{
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int lexer_errors=0;
#define YY_USER_ACTION do { \
    yylloc.first_line = yylineno; yylloc.last_line = yylineno; \
    yylloc.first_column = 1; yylloc.last_column = (int)yyleng + 1; \
} while (0);
static char *copy_text(const char *s){size_t n=strlen(s)+1;char *p=malloc(n);if(!p){perror("out of memory");exit(2);}memcpy(p,s,n);return p;}
%}
ID [A-Za-z_][A-Za-z0-9_]*
NUM [0-9]+(\.[0-9]+)?
%%
"int" { return INT; }
"float" { return FLOAT; }
"bool" { return BOOL; }
"if" { return IF; }
"else" { return ELSE; }
"while" { return WHILE; }
"print" { return PRINT; }
"true" { return TRUE; }
"false" { return FALSE; }
"==" { return EQ; }
"!=" { return NE; }
"<=" { return LE; }
">=" { return GE; }
"&&" { return AND; }
"||" { return OR; }
{NUM} { if(strchr(yytext,'.')){yylval.real=strtod(yytext,NULL);return FLOAT_LITERAL;} yylval.integer=strtol(yytext,NULL,10);return INT_LITERAL; }
{ID} { yylval.text=copy_text(yytext);return IDENT; }
"//"[^\n]* { }
"/*"([^*]|\*+[^*/])*"*"+"/" { }
[ \t\r]+ { }
\n { }
[{}();=+*/%<>!-] { return yytext[0]; }
. { fprintf(stderr,"line %d: lexical error: invalid character '%s'\n",yylineno,yytext); lexer_errors++; }
%%

