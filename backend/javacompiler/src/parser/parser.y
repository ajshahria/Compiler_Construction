/*
 * parser.y - Bison grammar of MiniJava.
 *
 * A program is the statements of the lab manual's language (section 5)
 * wrapped in a Java class with a main method:
 *         class Name { public static void main(String[] args) { ... } }
 * A bare list of statements is rejected, so C or C++ code is not accepted
 * as Java.
 *
 * Both `print x;` and `System.out.println(x);` print a value, and both
 * `bool` and `boolean` name the boolean type. for, do-while, switch and ++/--
 * are bonus features (manual section 14).
 *
 * Operator precedence and associativity are declared below (lowest first), so
 * the expression grammar has no conflicts. The dangling else binds to the
 * nearest if because ELSE has a higher precedence than LOWER_THAN_ELSE.
 *
 * Error recovery: after a syntax error the parser skips to the next ';' or '}'
 * and continues, so several errors are reported in one run.
 *
 * Written for Bison 2.4 or newer.
 */

%{
#include "ast/ast.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern int yylex();
void yyerror(const char* message);

Node* syntaxRoot = nullptr;   // the finished AST
int syntaxErrors = 0;         // number of syntax errors reported

/* Appends a statement to a block node. */
static Node* append(Node* block, Node* statement) {
    if (statement) block->children.push_back(statement);
    return block;
}

/* Builds a node whose children may be null (used by `for`). */
static Node* withSlots(const char* kind, int line, Node* a, Node* b, Node* c, Node* d) {
    Node* n = makeNode(kind, "", line);
    n->children = {a, b, c, d};
    return n;
}

/* Builds a declaration: decl(type) -> id [, initializer]. Takes ownership of name. */
static Node* declaration(const char* type, char* name, Node* init, int line) {
    Node* n = makeNode("decl", type, line, makeNode("id", name, line), init);
    std::free(name);
    return n;
}
%}

%error-verbose
%locations

%union {
    char* str;
    const char* type;
    struct Node* node;
}

%token <str> IDENT INT_LITERAL FLOAT_LITERAL
%token CLASS PUBLIC STATIC VOID MAIN STRING
%token INT FLOAT BOOLEAN
%token IF ELSE WHILE FOR DO SWITCH CASE DEFAULT BREAK PRINT PRINTLN
%token TRUE FALSE
%token EQ NE LE GE AND OR INC DEC

%type <type> type
%type <node> block statements statement expression
%type <node> for_init for_update optional_expression switch_cases case_clause

/* Precedence, lowest to highest. */
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%start program

%%

program
    : CLASS IDENT '{' PUBLIC STATIC VOID MAIN '(' STRING '[' ']' IDENT ')' block '}'
        { syntaxRoot = makeNode("program", $2, @1.first_line, $14); std::free($2); std::free($12); }
    ;

block
    : '{' statements '}'                     { $$ = $2; $$->line = @1.first_line; }
    ;

statements
    : /* empty */                            { $$ = makeNode("block", "", yylloc.first_line); }
    | statements statement                   { $$ = append($1, $2); }
    ;

type
    : INT                                    { $$ = "int"; }
    | FLOAT                                  { $$ = "float"; }
    | BOOLEAN                                { $$ = "boolean"; }
    ;

statement
    : type IDENT ';'                         { $$ = declaration($1, $2, nullptr, @2.first_line); }
    | type IDENT '=' expression ';'          { $$ = declaration($1, $2, $4, @2.first_line); }
    | IDENT '=' expression ';'               { $$ = makeNode("assign", $1, @1.first_line, $3); std::free($1); }
    | IDENT INC ';'                          { $$ = makeNode("inc", $1, @1.first_line); std::free($1); }
    | IDENT DEC ';'                          { $$ = makeNode("dec", $1, @1.first_line); std::free($1); }
    | PRINT expression ';'                   { $$ = makeNode("print", "", @1.first_line, $2); }
    | PRINTLN '(' expression ')' ';'         { $$ = makeNode("print", "", @1.first_line, $3); }
    | IF '(' expression ')' statement %prec LOWER_THAN_ELSE
                                             { $$ = makeNode("if", "", @1.first_line, $3, $5); }
    | IF '(' expression ')' statement ELSE statement
                                             { $$ = makeNode("if", "", @1.first_line, $3, $5, $7); }
    | WHILE '(' expression ')' statement     { $$ = makeNode("while", "", @1.first_line, $3, $5); }
    | FOR '(' for_init ';' optional_expression ';' for_update ')' statement
                                             { $$ = withSlots("for", @1.first_line, $3, $5, $7, $9); }
    | DO statement WHILE '(' expression ')' ';'
                                             { $$ = makeNode("do_while", "", @1.first_line, $2, $5); }
    | SWITCH '(' expression ')' '{' switch_cases '}'
                                             { $$ = makeNode("switch", "", @1.first_line, $3, $6); }
    | BREAK ';'                              { $$ = makeNode("break", "", @1.first_line); }
    | block                                  { $$ = $1; }
    | error ';'                              { $$ = nullptr; yyerrok; }
    | error '}'                              { $$ = nullptr; yyerrok; }
    ;

for_init
    : /* empty */                            { $$ = nullptr; }
    | IDENT '=' expression                   { $$ = makeNode("assign", $1, @1.first_line, $3); std::free($1); }
    | type IDENT '=' expression              { $$ = declaration($1, $2, $4, @2.first_line); }
    ;

for_update
    : /* empty */                            { $$ = nullptr; }
    | IDENT '=' expression                   { $$ = makeNode("assign", $1, @1.first_line, $3); std::free($1); }
    | IDENT INC                              { $$ = makeNode("inc", $1, @1.first_line); std::free($1); }
    | IDENT DEC                              { $$ = makeNode("dec", $1, @1.first_line); std::free($1); }
    ;

optional_expression
    : /* empty */                            { $$ = nullptr; }
    | expression                             { $$ = $1; }
    ;

switch_cases
    : /* empty */                            { $$ = makeNode("cases", "", yylloc.first_line); }
    | switch_cases case_clause               { $$ = append($1, $2); }
    ;

case_clause
    : CASE INT_LITERAL ':' statements        { $$ = makeNode("case", "", @1.first_line, makeNode("literal", $2, @2.first_line), $4); std::free($2); }
    | CASE TRUE ':' statements               { $$ = makeNode("case", "", @1.first_line, makeNode("literal", "true", @2.first_line), $4); }
    | CASE FALSE ':' statements              { $$ = makeNode("case", "", @1.first_line, makeNode("literal", "false", @2.first_line), $4); }
    | DEFAULT ':' statements                 { $$ = makeNode("default", "", @1.first_line, $3); }
    ;

expression
    : INT_LITERAL                            { $$ = makeNode("literal", $1, @1.first_line); std::free($1); }
    | FLOAT_LITERAL                          { $$ = makeNode("literal", $1, @1.first_line); std::free($1); }
    | TRUE                                   { $$ = makeNode("literal", "true", @1.first_line); }
    | FALSE                                  { $$ = makeNode("literal", "false", @1.first_line); }
    | IDENT                                  { $$ = makeNode("id", $1, @1.first_line); std::free($1); }
    | '(' expression ')'                     { $$ = $2; }
    | '-' expression %prec UMINUS            { $$ = makeNode("unary", "-", @1.first_line, $2); }
    | '!' expression                         { $$ = makeNode("unary", "!", @1.first_line, $2); }
    | expression '+' expression              { $$ = makeNode("binary", "+",  @2.first_line, $1, $3); }
    | expression '-' expression              { $$ = makeNode("binary", "-",  @2.first_line, $1, $3); }
    | expression '*' expression              { $$ = makeNode("binary", "*",  @2.first_line, $1, $3); }
    | expression '/' expression              { $$ = makeNode("binary", "/",  @2.first_line, $1, $3); }
    | expression '%' expression              { $$ = makeNode("binary", "%",  @2.first_line, $1, $3); }
    | expression '<' expression              { $$ = makeNode("binary", "<",  @2.first_line, $1, $3); }
    | expression '>' expression              { $$ = makeNode("binary", ">",  @2.first_line, $1, $3); }
    | expression LE expression               { $$ = makeNode("binary", "<=", @2.first_line, $1, $3); }
    | expression GE expression               { $$ = makeNode("binary", ">=", @2.first_line, $1, $3); }
    | expression EQ expression               { $$ = makeNode("binary", "==", @2.first_line, $1, $3); }
    | expression NE expression               { $$ = makeNode("binary", "!=", @2.first_line, $1, $3); }
    | expression AND expression              { $$ = makeNode("binary", "&&", @2.first_line, $1, $3); }
    | expression OR expression               { $$ = makeNode("binary", "||", @2.first_line, $1, $3); }
    ;

%%

/* Called by Bison on a syntax error. When the offending token starts a new
 * line and the previous line did not end with ';', '{' or '}', the mistake is
 * almost always a missing ';' at the end of the previous line, so that line is
 * reported instead. */
void yyerror(const char* message) {
    extern int previousTokenLine, previousToken;
    const char* prefix = "syntax error, ";
    bool previousLineComplete = previousToken == ';' || previousToken == '{' || previousToken == '}';

    if (std::strncmp(message, prefix, std::strlen(prefix)) == 0)
        message += std::strlen(prefix);

    if (std::strstr(message, "expecting CLASS") != nullptr)
        std::fprintf(stderr, "line %d: syntax error: %s (a program must be "
                     "'class Name { public static void main(String[] args) { ... } }')\n",
                     yylloc.first_line, message);
    else if (previousToken != 0 && yylloc.first_line > previousTokenLine && !previousLineComplete)
        std::fprintf(stderr, "line %d: syntax error: %s on line %d (missing ';' at the end of line %d?)\n",
                     previousTokenLine, message, yylloc.first_line, previousTokenLine);
    else
        std::fprintf(stderr, "line %d: syntax error: %s\n", yylloc.first_line, message);
    ++syntaxErrors;
}
