/*
 * parser.y - Bison grammar of the Tricora mini language.
 *
 * Implements the CFG in docs/grammar.md and builds the AST while parsing.
 * Operator precedence and associativity are declared below (lowest first),
 * which keeps the expression grammar short and conflict-free. The classic
 * dangling-else is resolved by giving ELSE a higher precedence than the
 * if-without-else rule, so an else binds to the nearest if.
 *
 * Error recovery: the `error` token lets the parser skip to the next ';' or
 * '}' after a syntax error, so several syntax errors can be reported in one run.
 *
 * Written for Bison 2.4 or newer.
 */

%{
#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yylex(void);
void yyerror(const char *message);

extern int yylineno;
StmtList *program_root = NULL;   /* the finished AST */
int parser_errors = 0;            /* number of syntax errors reported */
%}

%error-verbose
%locations

%union {
    char *text;
    long integer;
    double real;
    Type type;
    Expr *expr;
    Stmt *stmt;
    StmtList *list;
}

%token <text>    IDENT
%token <integer> INT_LITERAL
%token <real>    FLOAT_LITERAL
%token INT FLOAT BOOL IF ELSE WHILE PRINT TRUE FALSE
%token EQ NE LE GE AND OR

%type <type> type
%type <expr> expression
%type <stmt> statement block
%type <list> statement_list

/* Precedence, lowest to highest. */
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS

/* Dangling else: shifting ELSE is preferred over reducing a plain if. */
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE

%start program

%%

program
    : statement_list                         { program_root = $1; }
    ;

statement_list
    : /* empty */                            { $$ = ast_list_new(); }
    | statement_list statement               { $$ = ast_list_append($1, $2); }
    ;

statement
    : type IDENT ';'                         { $$ = ast_decl($1, $2, NULL, @2.first_line); }
    | type IDENT '=' expression ';'          { $$ = ast_decl($1, $2, $4, @2.first_line); }
    | IDENT '=' expression ';'               { $$ = ast_assign($1, $3, @1.first_line); }
    | PRINT expression ';'                   { $$ = ast_print($2, @1.first_line); }
    | IF '(' expression ')' statement %prec IF_WITHOUT_ELSE
                                             { $$ = ast_if($3, $5, NULL, @1.first_line); }
    | IF '(' expression ')' statement ELSE statement
                                             { $$ = ast_if($3, $5, $7, @1.first_line); }
    | WHILE '(' expression ')' statement     { $$ = ast_while($3, $5, @1.first_line); }
    | block                                  { $$ = $1; }
    | error ';'                              { $$ = ast_error(@1.first_line); yyerrok; }
    | error '}'                              { $$ = ast_error(@1.first_line); yyerrok; }
    ;

block
    : '{' statement_list '}'                 { $$ = ast_block($2, @1.first_line); }
    ;

type
    : INT                                    { $$ = TYPE_INT; }
    | FLOAT                                  { $$ = TYPE_FLOAT; }
    | BOOL                                   { $$ = TYPE_BOOL; }
    ;

expression
    : INT_LITERAL                            { $$ = ast_int($1, @1.first_line); }
    | FLOAT_LITERAL                          { $$ = ast_float($1, @1.first_line); }
    | TRUE                                   { $$ = ast_bool(1, @1.first_line); }
    | FALSE                                  { $$ = ast_bool(0, @1.first_line); }
    | IDENT                                  { $$ = ast_name($1, @1.first_line); }
    | '(' expression ')'                     { $$ = $2; }
    | expression '+' expression              { $$ = ast_binary(OP_ADD, $1, $3, @2.first_line); }
    | expression '-' expression              { $$ = ast_binary(OP_SUB, $1, $3, @2.first_line); }
    | expression '*' expression              { $$ = ast_binary(OP_MUL, $1, $3, @2.first_line); }
    | expression '/' expression              { $$ = ast_binary(OP_DIV, $1, $3, @2.first_line); }
    | expression '%' expression              { $$ = ast_binary(OP_MOD, $1, $3, @2.first_line); }
    | expression '<' expression              { $$ = ast_binary(OP_LT,  $1, $3, @2.first_line); }
    | expression '>' expression              { $$ = ast_binary(OP_GT,  $1, $3, @2.first_line); }
    | expression LE expression               { $$ = ast_binary(OP_LE,  $1, $3, @2.first_line); }
    | expression GE expression               { $$ = ast_binary(OP_GE,  $1, $3, @2.first_line); }
    | expression EQ expression               { $$ = ast_binary(OP_EQ,  $1, $3, @2.first_line); }
    | expression NE expression               { $$ = ast_binary(OP_NE,  $1, $3, @2.first_line); }
    | expression AND expression              { $$ = ast_binary(OP_AND, $1, $3, @2.first_line); }
    | expression OR expression               { $$ = ast_binary(OP_OR,  $1, $3, @2.first_line); }
    | '!' expression                         { $$ = ast_unary(OP_NOT, $2, @1.first_line); }
    | '-' expression %prec UMINUS            { $$ = ast_unary(OP_NEG, $2, @1.first_line); }
    ;

%%

/* Called by Bison on a syntax error. yylloc holds the offending token's line.
 * When that token starts a new line and the previous line did not end with
 * ';', '{' or '}', the real mistake is almost always a missing ';' at the end
 * of the previous line, so that line is reported. */
void yyerror(const char *message)
{
    extern int previous_token_line, previous_token;
    const char *prefix = "syntax error, ";
    int previous_line_complete = previous_token == ';' || previous_token == '{' || previous_token == '}';

    if (strncmp(message, prefix, strlen(prefix)) == 0)
        message += strlen(prefix);          /* avoid "syntax error: syntax error" */

    if (yylloc.first_line > previous_token_line && !previous_line_complete)
        fprintf(stderr, "line %d: syntax error: %s on line %d (missing ';' at the end of line %d?)\n",
                previous_token_line, message, yylloc.first_line, previous_token_line);
    else
        fprintf(stderr, "line %d: syntax error: %s\n", yylloc.first_line, message);
    parser_errors++;
}
