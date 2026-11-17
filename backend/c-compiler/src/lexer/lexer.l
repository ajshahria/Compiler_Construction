/*
 * lexer.l - Flex specification of the Tricora lexical analyzer.
 *
 * Turns the source text into tokens for the Bison parser. Keywords are listed
 * before the identifier pattern so that Flex's "first rule wins" tie-break
 * makes `while` a keyword and not an identifier. Comments and whitespace are
 * discarded. Anything that is not a valid token is reported as a lexical error
 * with its line number, and scanning continues so later errors are found too.
 */

%option noyywrap yylineno nounput noinput

%{
#include "ast.h"
#include "parser.tab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int lexer_errors = 0;          /* number of lexical errors reported */
static int comment_start = 0;  /* line where an open block comment began */

/* Every token records the line it starts on (used for error messages). */
#define YY_USER_ACTION  yylloc.first_line = yylloc.last_line = yylineno;

/* Flex generates scan_token(); yylex() below wraps it to remember the line of
 * the previous token, so "missing ';'" can be reported on the line where the
 * semicolon belongs instead of the line of the next token. */
#define YY_DECL int scan_token(void)
int previous_token_line = 1;
int previous_token = 0;

static void lexical_error(const char *message, const char *text)
{
    fprintf(stderr, "line %d: lexical error: %s '%s'\n", yylineno, message, text);
    lexer_errors++;
}

static char *copy_text(const char *text)
{
    char *copy = malloc(strlen(text) + 1);
    if (copy == NULL) {
        perror("out of memory");
        exit(2);
    }
    strcpy(copy, text);
    return copy;
}
%}

DIGIT   [0-9]
LETTER  [A-Za-z_]
ID      {LETTER}({LETTER}|{DIGIT})*

%x COMMENT

%%

 /* ----- comments and whitespace (discarded) ----- */
"//"[^\n]*              { /* single-line comment */ }
"/*"                    { comment_start = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>[^*\n]+        { }
<COMMENT>"*"            { }
<COMMENT>\n             { }
<COMMENT><<EOF>>        {
                          fprintf(stderr, "line %d: lexical error: unterminated block comment\n", comment_start);
                          lexer_errors++;
                          BEGIN(INITIAL);
                          return 0;
                        }
[ \t\r\n]+              { }

 /* ----- keywords (must come before the identifier rule) ----- */
"int"                   { return INT; }
"float"                 { return FLOAT; }
"bool"                  { return BOOL; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"print"                 { return PRINT; }
"true"                  { return TRUE; }
"false"                 { return FALSE; }

 /* ----- multi-character operators ----- */
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
">="                    { return GE; }
"&&"                    { return AND; }
"||"                    { return OR; }

 /* ----- literals and identifiers ----- */
{DIGIT}+"."{DIGIT}+     { yylval.real = strtod(yytext, NULL); return FLOAT_LITERAL; }
{DIGIT}+                { yylval.integer = strtol(yytext, NULL, 10); return INT_LITERAL; }
{DIGIT}+{ID}            {
                          /* Report it, then pass it on as an identifier so the
                           * parser does not add a second, misleading error. */
                          lexical_error("invalid identifier (cannot start with a digit)", yytext);
                          yylval.text = copy_text(yytext);
                          return IDENT;
                        }
{ID}                    { yylval.text = copy_text(yytext); return IDENT; }

 /* ----- single-character operators and delimiters ----- */
[-+*/%<>=!(){};]        { return yytext[0]; }

 /* ----- anything else is an invalid token ----- */
.                       { lexical_error("invalid character", yytext); }

%%

int yylex(void)
{
    static int current_token_line = 1;
    static int current_token = 0;
    previous_token_line = current_token_line;
    previous_token = current_token;
    current_token = scan_token();
    current_token_line = yylloc.first_line;
    return current_token;
}
