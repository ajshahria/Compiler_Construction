/* lexer.l - Phase 1: Lexical analysis (Flex).
 * Turns characters into tokens, discards comments/whitespace,
 * reports invalid tokens with the line number.
 */
%{
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "ast.h"
#include "diagnostics.h"
#include "headers.h"
#include "parser.tab.h"

int g_dumpTokens = 0;
int g_lastLexErrorLine = 0;   /* used by the parser to suppress follow-up syntax errors */
static int commentStart = 0;   /* set by main (--tokens) to print every token */

/* Print the token when --tokens is on, then hand it to the parser. */
/* Turn the characters between the quotes of "..." or '.' into the real text (\n, \t, \\, \" ...). */
static char* unescape(const char* s, int len) {
    std::string out;
    for (int i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            i++;
            switch (s[i]) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case '0': out += '\0'; break;
                default:  out += s[i]; break;   /* \\ \" \' and anything else */
            }
        } else {
            out += s[i];
        }
    }
    return strdup(out.c_str());
}

static bool seenCode = false;   /* true once any real token (not #include) was returned */

/* #include must come before any code: report it when it does not. */
static void checkIncludeAtTop(const std::string& name, int line) {
    if (seenCode)
        reportError(PH_LEXICAL, line, "#include <" + name +
                    "> must appear at the top of the file, before any code");
}

static int tok(int t, const char* name) {
    seenCode = true;
    if (g_dumpTokens)
        printf("%-12s %-10s line %d\n", name, yytext, yylineno);
    return t;
}
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
#define TOK(t) return tok(t, #t)
%}

%option noyywrap
%option nounput
%option yylineno

DIGIT   [0-9]
LETTER  [A-Za-z_]

%x BLOCKCOMMENT

%%

"int"       { TOK(KW_INT); }
"float"     { TOK(KW_FLOAT); }
"bool"      { TOK(KW_BOOL); }
"if"        { TOK(KW_IF); }
"else"      { TOK(KW_ELSE); }
"while"     { TOK(KW_WHILE); }
"print"     { TOK(KW_PRINT); }
"true"      { TOK(KW_TRUE); }
"false"     { TOK(KW_FALSE); }
"for"       { TOK(KW_FOR); }
"do"        { TOK(KW_DO); }
"switch"    { TOK(KW_SWITCH); }
"case"      { TOK(KW_CASE); }
"default"   { TOK(KW_DEFAULT); }
"break"     { TOK(KW_BREAK); }
"continue"  { TOK(KW_CONTINUE); }
"return"    { TOK(KW_RETURN); }
"main"      { TOK(KW_MAIN); }
"void"      { TOK(KW_VOID); }
"const"     { TOK(KW_CONST); }
"double"    { TOK(KW_DOUBLE); }
"long"      { TOK(KW_LONG); }
"short"     { TOK(KW_SHORT); }
"cin"       { TOK(KW_CIN); }
"std::cin"  { TOK(KW_CIN); }
"cout"      { TOK(KW_COUT); }
"std::cout" { TOK(KW_COUT); }
"endl"      { TOK(KW_ENDL); }
"std::endl" { TOK(KW_ENDL); }

({DIGIT}+"."{DIGIT}*|"."{DIGIT}+)([eE][+\-]?{DIGIT}+)?  { yylval.fval = atof(yytext); TOK(FLOAT_LIT); }
{DIGIT}+[eE][+\-]?{DIGIT}+                              { yylval.fval = atof(yytext); TOK(FLOAT_LIT); }
{DIGIT}+                                                { yylval.ival = atol(yytext); TOK(INT_LIT); }

\"([^\"\\\n]|\\.)*\"  { yylval.sval = unescape(yytext + 1, yyleng - 2); TOK(STRING_LIT); }
\"([^\"\\\n]|\\.)*    {
    g_lastLexErrorLine = yylineno;
    reportError(PH_LEXICAL, yylineno, "unterminated string literal");
}
'([^'\\\n]|\\.)'      { yylval.sval = unescape(yytext + 1, yyleng - 2); TOK(STRING_LIT); }

{DIGIT}+{LETTER}({LETTER}|{DIGIT})* {
    g_lastLexErrorLine = yylineno;
    reportError(PH_LEXICAL, yylineno,
                std::string("invalid identifier '") + yytext + "' (cannot start with a digit)");
}

{LETTER}({LETTER}|{DIGIT})* {
    yylval.sval = strdup(yytext);
    TOK(IDENTIFIER);
}

^[ \t]*"#"[ \t]*"include"[ \t]*"<"[^>\n]+">"[ \t]*  {
    /* #include <name>: take the name between < and > */
    std::string t(yytext);
    size_t a = t.find('<'), b = t.find('>');
    checkIncludeAtTop(t.substr(a + 1, b - a - 1), yylineno);
    includeHeader(t.substr(a + 1, b - a - 1), true, yylineno);
}

^[ \t]*"#"[ \t]*"include"[ \t]*\"[^\"\n]+\"[ \t]*  {
    std::string t(yytext);
    size_t a = t.find('"'), b = t.rfind('"');
    includeHeader(t.substr(a + 1, b - a - 1), false, yylineno);
}

"using"[ \t\r\n]+"namespace"[ \t\r\n]+"std"[ \t]*";"  { /* accepted and ignored */ }

"<<"    { TOK(SHL); }
">>"    { TOK(SHR); }
"++"    { TOK(INC); }
"--"    { TOK(DEC); }
"+="    { TOK(ADDEQ); }
"-="    { TOK(SUBEQ); }
"*="    { TOK(MULEQ); }
"/="    { TOK(DIVEQ); }
"<="    { TOK(LE); }
">="    { TOK(GE); }
"=="    { TOK(EQ); }
"!="    { TOK(NE); }
"&&"    { TOK(AND); }
"||"    { TOK(OR); }

[+\-*/%<>=!(){};:,?]  { return tok(yytext[0], yytext); }

"//".*              { /* single-line comment: discarded */ }
"/*"                { commentStart = yylineno; BEGIN(BLOCKCOMMENT); }
<BLOCKCOMMENT>"*/"  { BEGIN(INITIAL); }
<BLOCKCOMMENT>\n    { /* yylineno is updated by %option yylineno */ }
<BLOCKCOMMENT>.     { }
<BLOCKCOMMENT><<EOF>> {
    reportError(PH_LEXICAL, commentStart, "unterminated block comment (started here)");
    BEGIN(INITIAL);
    return 0;
}

\n                  { /* newline: kept separate so '^' (start of line) works for indented #include */ }
[ \t\r]+            { /* whitespace: discarded */ }

.   {
    g_lastLexErrorLine = yylineno;
    reportError(PH_LEXICAL, yylineno,
                std::string("invalid character '") + yytext + "'");
}

%%
