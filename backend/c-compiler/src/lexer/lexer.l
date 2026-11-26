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

/* ----- #include <header> (C style) ----- */

/* The C standard headers this compiler accepts, with what each one is for.
 * Only <stdio.h> has an effect (it declares printf); the others are accepted
 * so ordinary C programs compile, and are listed by --headers. */
typedef struct { const char *name; const char *category; const char *purpose; } KnownHeader;

static const KnownHeader KNOWN_HEADERS[] = {
    { "stdio.h",   "I/O",          "printf, scanf" },
    { "stdlib.h",  "Utilities",    "malloc, rand, exit" },
    { "string.h",  "Strings",      "strlen, strcpy, strcmp" },
    { "ctype.h",   "Strings",      "character classification" },
    { "math.h",    "Math",         "sqrt, pow, sin" },
    { "time.h",    "Utilities",    "date and time" },
    { "assert.h",  "Diagnostics",  "assert macro" },
    { "errno.h",   "Diagnostics",  "error numbers" },
    { "stdbool.h", "Type support", "bool, true, false" },
    { "stddef.h",  "Type support", "size_t, NULL" },
    { "stdint.h",  "Type support", "fixed-width integers" },
    { "limits.h",  "Type support", "integer limits" },
    { "float.h",   "Type support", "float limits" },
    { NULL, NULL, NULL }
};

/* Every header included so far, in order, with the line of its #include. */
#define MAX_HEADERS 32
static const KnownHeader *included[MAX_HEADERS];
static int included_line[MAX_HEADERS];
static int included_count = 0;

int header_included(const char *name)
{
    for (int k = 0; k < included_count; k++)
        if (strcmp(included[k]->name, name) == 0)
            return 1;
    return 0;
}

/* --headers: a table of the included headers (preprocessor step). */
void print_headers(FILE *out)
{
    if (included_count == 0) {
        fprintf(out, "(no headers included)\n");
        return;
    }
    fprintf(out, "%-14s %-5s %-13s %s\n", "Header", "Line", "Category", "Role in this compiler");
    fprintf(out, "------------------------------------------------------------------------\n");
    for (int k = 0; k < included_count; k++) {
        char shown[40];
        snprintf(shown, sizeof shown, "<%s>", included[k]->name);
        const char *role = strcmp(included[k]->name, "stdio.h") == 0
                               ? "enables printf" : "accepted (no effect)";
        fprintf(out, "%-14s %-5d %-13s %s  - %s\n", shown, included_line[k],
                included[k]->category, role, included[k]->purpose);
    }
}

/* A line starting with '#'. Handles   #include <name>   (spaces allowed) and
 * reports anything else. Returns how many characters of the line it used. */
static int preprocessor_line(const char *line)
{
    const char *c = line + 1;
    while (*c == ' ' || *c == '\t') c++;
    if (strncmp(c, "include", 7) != 0) {
        lexical_error("unsupported preprocessor directive", line);
        return (int)strlen(line);
    }
    c += 7;
    while (*c == ' ' || *c == '\t') c++;

    char quote = *c == '<' ? '>' : *c == '"' ? '"' : 0;
    const char *close = quote ? strchr(c + 1, quote) : NULL;
    if (close == NULL) {
        lexical_error("malformed #include (expected #include <header>)", line);
        return (int)strlen(line);
    }
    int used = (int)(close + 1 - line);
    char text[128], name[64];
    snprintf(text, sizeof text, "%.*s", used, line);
    if (quote == '"') {
        lexical_error("user header files are not supported (only <standard> headers)", text);
        return used;
    }
    snprintf(name, sizeof name, "%.*s", (int)(close - c - 1), c + 1);
    for (int k = 0; KNOWN_HEADERS[k].name != NULL; k++) {
        if (strcmp(KNOWN_HEADERS[k].name, name) == 0) {
            /* a repeated #include of the same header is ignored */
            if (!header_included(name) && included_count < MAX_HEADERS) {
                included[included_count] = &KNOWN_HEADERS[k];
                included_line[included_count++] = yylineno;
            }
            return used;
        }
    }
    lexical_error("unknown header", text);
    return used;
}

/* A string literal may only use the escapes \n \t \r \0 \\ \" and \'. */
static void check_escapes(const char *text)
{
    for (const char *c = text; *c != '\0'; c++) {
        if (*c != '\\')
            continue;
        c++;
        if (strchr("ntr0\\\"'", *c) == NULL) {
            char bad[3] = { '\\', *c, '\0' };
            lexical_error("unsupported escape sequence in string literal", bad);
        }
    }
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

 /* ----- #include lines (C style): the header is recorded, no token.
  * One rule reads the whole line; what follows the directive (spaces, a
  * comment, a Windows '\r') is given back to the scanner with yyless. ----- */
"#"[^\r\n]*             {
                          int used = preprocessor_line(yytext);
                          if (used < yyleng)
                              yyless(used);
                        }

 /* ----- string literals (only in printf) ----- */
\"(\\.|[^"\\\n])*\"     {
                          /* the text between the quotes, escapes kept */
                          yylval.text = copy_text(yytext + 1);
                          yylval.text[yyleng - 2] = '\0';
                          check_escapes(yylval.text);
                          return STRING_LITERAL;
                        }
\"(\\.|[^"\\\n])*       { lexical_error("unterminated string literal", yytext); }

 /* ----- keywords (must come before the identifier rule) ----- */
"return"                { return RETURN; }
"void"                  { return VOID; }
"printf"                { return PRINTF; }
"scanf"                 { return SCANF; }
"int"                   { return INT; }
"float"                 { return FLOAT; }
"bool"                  { return BOOL; }
"if"                    { return IF; }
"else"                  { return ELSE; }
"while"                 { return WHILE; }
"for"                   { return FOR; }         /* loops and switch (bonus) */
"do"                    { return DO; }
"switch"                { return SWITCH; }
"case"                  { return CASE; }
"default"               { return DEFAULT; }
"break"                 { return BREAK; }
"continue"              { return CONTINUE; }
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
"++"                    { return INC; }         /* increment/decrement (bonus) */
"--"                    { return DEC; }
"+="                    { return ADD_ASSIGN; }  /* compound assignment (bonus) */
"-="                    { return SUB_ASSIGN; }
"*="                    { return MUL_ASSIGN; }
"/="                    { return DIV_ASSIGN; }
"%="                    { return MOD_ASSIGN; }

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
[-+*/%<>=!(){};,&\[\]:] { return yytext[0]; }   /* '&' only in scanf(..., &x);
                                                     [ ] arrays, : case labels */

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
