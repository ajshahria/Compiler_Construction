
%option noyywrap yylineno never-interactive

%{
#include "parser.tab.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool hadLexicalError = false;

static char* copyText(const char* s) {
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    memcpy(p, s, n);
    return p;
}
%}

ID  [A-Za-z_$][A-Za-z0-9_$]*
NUM [0-9]+(\.[0-9]+)?([eE][+-]?[0-9]+)?[fFdD]?

%%

[ \t\r\n]+                 ;
"//"[^\n]*                 ;
"/*"([^*]|\*+[^*/])*"*/"  ;

"class"                    return CLASS;
"public"                   return PUBLIC;
"static"                   return STATIC;
"void"                     return VOID;
"main"                     return MAIN;
"String"                   return STRING;
"int"                      return INT;
"float"                    return FLOAT;
"boolean"                  return BOOLEAN;

"if"                       return IF;
"else"                     return ELSE;
"while"                    return WHILE;

"true"                     return TRUE;
"false"                    return FALSE;

"System.out.println"       return PRINT;

"=="                       return EQ;
"!="                       return NE;
"<="                       return LE;
">="                       return GE;
"&&"                       return AND;
"||"                       return OR;

{NUM} {
    yylval.str = copyText(yytext);
    return NUMBER;
}

{ID} {
    yylval.str = copyText(yytext);
    return IDENT;
}

[{}()\[\];=+*/%<>!-] {
    return yytext[0];
}

. {
    fprintf(stderr,
        "line %d: invalid character '%s'\n",
        yylineno, yytext);
    hadLexicalError = true;

    return INVALID;
}

%%
