/*
 * lexer.l - Flex specification of the MiniJava lexical analyzer.
 *
 * Recognises the lab-manual language (int, float, bool, if, else, while,
 * print, true, false) plus the Java spellings of the same ideas (boolean,
 * System.out.println, the class/main wrapper) and the bonus statements
 * (for, do-while, switch/case/default/break, ++, --).
 *
 * Keywords are listed before the identifier rule so Flex's "first rule wins"
 * tie-break makes `while` a keyword. Comments and whitespace are discarded.
 * Every invalid token is reported with its line number and skipped, so the
 * whole file is scanned and every lexical error is reported.
 */

%option noyywrap yylineno never-interactive nounput noinput

%{
#include "parser.tab.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

int lexicalErrors = 0;          // number of lexical errors reported
static int commentStartLine = 0;

/* Every token records the line it starts on. */
#define YY_USER_ACTION  yylloc.first_line = yylloc.last_line = yylineno;

/* Flex generates scanToken(); yylex() at the bottom wraps it to remember the
 * previous token, which the parser uses to place "missing ';'" errors. */
#define YY_DECL int scanToken()
int previousTokenLine = 1;
int previousToken = 0;

static void lexicalError(const char* message, const char* text) {
    std::fprintf(stderr, "line %d: lexical error: %s '%s'\n", yylineno, message, text);
    ++lexicalErrors;
}

static char* copyText(const char* text) {
    char* copy = static_cast<char*>(std::malloc(std::strlen(text) + 1));
    if (!copy) {
        std::perror("out of memory");
        std::exit(2);
    }
    std::strcpy(copy, text);
    return copy;
}
%}

DIGIT   [0-9]
LETTER  [A-Za-z_]
ID      {LETTER}({LETTER}|{DIGIT})*
INTNUM  {DIGIT}+
REALNUM {DIGIT}+"."{DIGIT}+([eE][+-]?{DIGIT}+)?

%x COMMENT

%%

 /* ----- comments and whitespace (discarded) ----- */
"//"[^\n]*                  { }
"/*"                        { commentStartLine = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"               { BEGIN(INITIAL); }
<COMMENT>[^*\n]+            { }
<COMMENT>"*"                { }
<COMMENT>\n                 { }
<COMMENT><<EOF>>            {
                              std::fprintf(stderr, "line %d: lexical error: unterminated block comment\n",
                                           commentStartLine);
                              ++lexicalErrors;
                              BEGIN(INITIAL);
                              return 0;
                            }
[ \t\r\n]+                  { }

 /* ----- Java program wrapper ----- */
"class"                     { return CLASS; }
"public"                    { return PUBLIC; }
"static"                    { return STATIC; }
"void"                      { return VOID; }
"main"                      { return MAIN; }
"String"                    { return STRING; }

 /* ----- types ----- */
"int"                       { return INT; }
"float"                     { return FLOAT; }
"bool"                      { return BOOLEAN; }   /* manual spelling */
"boolean"                   { return BOOLEAN; }   /* Java spelling   */

 /* ----- statements ----- */
"if"                        { return IF; }
"else"                      { return ELSE; }
"while"                     { return WHILE; }
"for"                       { return FOR; }
"do"                        { return DO; }
"switch"                    { return SWITCH; }
"case"                      { return CASE; }
"default"                   { return DEFAULT; }
"break"                     { return BREAK; }
"print"                     { return PRINT; }      /* manual: print x;              */
"System.out.println"        { return PRINTLN; }    /* Java:   System.out.println(x); */

 /* ----- boolean literals ----- */
"true"                      { return TRUE; }
"false"                     { return FALSE; }

 /* ----- multi-character operators ----- */
"=="                        { return EQ; }
"!="                        { return NE; }
"<="                        { return LE; }
">="                        { return GE; }
"&&"                        { return AND; }
"||"                        { return OR; }
"++"                        { return INC; }
"--"                        { return DEC; }

 /* ----- literals and identifiers ----- */
{REALNUM}                   { yylval.str = copyText(yytext); return FLOAT_LITERAL; }
{INTNUM}                    { yylval.str = copyText(yytext); return INT_LITERAL; }
{DIGIT}+{ID}                {
                              /* Report it, then pass it on as an identifier so
                               * the parser does not add a misleading error. */
                              lexicalError("invalid identifier (cannot start with a digit)", yytext);
                              yylval.str = copyText(yytext);
                              return IDENT;
                            }
{ID}                        { yylval.str = copyText(yytext); return IDENT; }

 /* ----- single-character operators and delimiters ----- */
[-+*/%<>=!(){}\[\];:]       { return yytext[0]; }

 /* ----- anything else is an invalid token ----- */
.                           { lexicalError("invalid character", yytext); }

%%

int yylex() {
    static int currentTokenLine = 1;
    static int currentToken = 0;
    previousTokenLine = currentTokenLine;
    previousToken = currentToken;
    currentToken = scanToken();
    currentTokenLine = yylloc.first_line;
    return currentToken;
}
