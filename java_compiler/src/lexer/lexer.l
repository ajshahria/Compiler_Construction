%option noyywrap yylineno never-interactive
%{
#include "parser.tab.hpp"
#include <cstdlib>
#include <cstring>
static char* copyText(const char* s){size_t n=strlen(s)+1;char* p=(char*)malloc(n);memcpy(p,s,n);return p;}
#define RET(t) return t
%}
ID [A-Za-z_$][A-Za-z0-9_$]*
NUM [0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?[fFdD]?
%%
[ \t\r\n]+                 ;
"//"[^\n]*                 ;
"/*"([^*]|\*+[^*/])*"*/" ;
"class"                    {RET(CLASS);}
"public"                   {RET(PUBLIC);}
"static"                   {RET(STATIC);}
"void"                     {RET(VOID);}
"main"                     {RET(MAIN);}
"String"                   {RET(STRING);}
"int"                      {RET(INT);}
"float"                    {RET(FLOAT);}
"boolean"                  {RET(BOOLEAN);}
"if"                       {RET(IF);}
"else"                     {RET(ELSE);}
"while"                    {RET(WHILE);}
"true"                     {RET(TRUE);}
"false"                    {RET(FALSE);}
"System.out.println"       {RET(PRINT);}
"=="                       {RET(EQ);}
"!="                       {RET(NE);}
"<="                       {RET(LE);}
">="                       {RET(GE);}
"&&"                       {RET(AND);}
"||"                       {RET(OR);}
{NUM}                      { yylval.str=copyText(yytext); RET(NUMBER); }
{ID}                       { yylval.str=copyText(yytext); RET(IDENT); }
[{}()\[\];=+*/%<>!-]       { return yytext[0]; }
.                          { fprintf(stderr,"line %d: invalid character '%s'\n",yylineno,yytext); }
%%
