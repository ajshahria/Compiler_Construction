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
#include "imports/imports.h"
#include "parser.tab.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

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

/* True once the scan has returned its first token. Imports must come before
 * any code, as in Java. Cleared at end of file, so a second scan (--all scans
 * the file twice) starts fresh. */
static bool codeStarted = false;

/* An 'import [static] name;' line (the lexeme of the import rule). The
 * import is recorded for --headers; no token is returned. */
static void importLine(const char* text) {
    std::string line = text;
    std::string::size_type p = 6;                       // after "import"
    auto skipSpace = [&] { while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) ++p; };
    skipSpace();
    bool isStatic = line.compare(p, 6, "static") == 0 && p + 6 < line.size() &&
                    (line[p + 6] == ' ' || line[p + 6] == '\t');
    if (isStatic) { p += 6; skipSpace(); }
    std::string name;
    while (p < line.size() && line[p] != ';' && line[p] != ' ' && line[p] != '\t') name += line[p++];
    if (codeStarted) {
        lexicalError("import must come before the class (move it to the top of the file)", text);
        return;
    }
    addImport(name, isStatic, yylineno);
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
QNAME   {ID}([ \t]*"."[ \t]*{ID})*
WS      [ \t]

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

 /* ----- import lines (Java's #include): recorded for --headers, no token.
  * A well-formed import comes first; anything else after 'import ' is a
  * malformed import. Both stop at the end of the line. ----- */
"import"{WS}+("static"{WS}+)?{QNAME}({WS}*"."{WS}*"*")?{WS}*";"  { importLine(yytext); }
"import"{WS}+[^;\n]*";"?    { lexicalError("malformed import (expected import java.package.Name;)", yytext); }

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
"continue"                  { return CONTINUE; }    /* bonus */
"return"                    { return RETURN; }      /* methods (bonus) */
"print"                     { return PRINT; }      /* manual: print x;              */
"System.out.println"        { return PRINTLN; }    /* Java:   System.out.println(x); */

 /* ----- input: Scanner sc = new Scanner(System.in);  int n = sc.nextInt(); ----- */
"Scanner"                   { return SCANNER; }
"new"                       { return NEW; }
"System.in"                 { return SYSTEM_IN; }
{ID}"."("nextInt"|"nextFloat"|"nextDouble"|"nextBoolean")  {
                              yylval.str = copyText(yytext);   /* e.g. "sc.nextInt" */
                              return SCANNER_READ;
                            }
{ID}".length"               {
                              /* arrays (bonus): a.length is the number of elements */
                              yylval.str = copyText(yytext);
                              yylval.str[yyleng - 7] = '\0';   /* keep only the array name */
                              return LENGTH;
                            }
{ID}"."{ID}                 {
                              /* any other method call: reported, then passed on as a
                               * read so the parser does not add a misleading error */
                              lexicalError("unsupported method (only nextInt, nextFloat, nextDouble "
                                           "and nextBoolean of a Scanner are supported)", yytext);
                              yylval.str = copyText(yytext);
                              return SCANNER_READ;
                            }

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
[-+*/%<>=!(){}\[\];:,]      { return yytext[0]; }   /* ',' separates arguments (bonus) */

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
    codeStarted = currentToken != 0;   // reset at end of file for a second scan
    return currentToken;
}
