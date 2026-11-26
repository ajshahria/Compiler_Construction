/*
 * parser.y - Bison grammar of the Tricora mini language.
 *
 * Implements the CFG in docs/grammar.md and builds the AST while parsing.
 * Operator precedence and associativity are declared below (lowest first),
 * which keeps the expression grammar short and conflict-free. The classic
 * dangling-else is resolved by giving ELSE a higher precedence than the
 * if-without-else rule, so an else binds to the nearest if.
 *
 * C style (bonus): besides the manual's plain list of statements, a program
 * may be written as C, with #include lines (handled by the lexer), an
 * `int main() { ... }` function, `return` and `printf("...", ...)`. Rules a
 * grammar cannot express (only one function, called main, at the top level)
 * are checked by the semantic analyzer.
 *
 * Bonus language extensions: for, do-while, switch/case/default, break,
 * continue, x++ / x-- / ++x / --x, compound assignment (+= -= *= /= %=),
 * one-dimensional arrays (int a[10]; a[i] = e; int b[] = {1, 2}) and
 * functions with parameters, calls and return. Increment, decrement and
 * compound assignment are statements (not expressions), as in the manual's
 * language every assignment is a statement.
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
    ExprList *args;
    ParamList *params;
}

%token <text>    IDENT
%token <integer> INT_LITERAL
%token <real>    FLOAT_LITERAL
%token INT FLOAT BOOL IF ELSE WHILE PRINT TRUE FALSE
%token EQ NE LE GE AND OR
%token <text>    STRING_LITERAL
%token RETURN VOID PRINTF      /* C style (bonus): int main(), return, printf */
%token SCANF                   /* C style (bonus): scanf("%d", &x) reads input */
%token FOR DO SWITCH CASE DEFAULT BREAK CONTINUE        /* loops and switch (bonus) */
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN

%type <type> type
%type <expr> expression
%type <stmt> statement block simple_statement for_init for_update
%type <list> statement_list
%type <args> printf_args scanf_args call_args arg_list initializer
%type <expr> lvalue for_condition
%type <params> parameters parameter_list
%type <integer> case_value

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
    | type IDENT '[' INT_LITERAL ']' ';'     { $$ = ast_array_decl($1, $2, $4, NULL, @2.first_line); }
    | type IDENT '[' INT_LITERAL ']' '=' initializer ';'
                                             { $$ = ast_array_decl($1, $2, $4, $7, @2.first_line); }
    | type IDENT '[' ']' '=' initializer ';' { $$ = ast_array_decl($1, $2, 0, $6, @2.first_line); }
    | simple_statement ';'                   { $$ = $1; }
    | PRINT expression ';'                   { $$ = ast_print($2, @1.first_line); }
    | IF '(' expression ')' statement %prec IF_WITHOUT_ELSE
                                             { $$ = ast_if($3, $5, NULL, @1.first_line); }
    | IF '(' expression ')' statement ELSE statement
                                             { $$ = ast_if($3, $5, $7, @1.first_line); }
    | WHILE '(' expression ')' statement     { $$ = ast_while($3, $5, @1.first_line); }
    | block                                  { $$ = $1; }
    /* ----- loops and switch (bonus) ----- */
    | DO statement WHILE '(' expression ')' ';'
                                             { $$ = ast_do_while($2, $5, @1.first_line); }
    | DO statement WHILE '(' expression ')' error
                                             { $$ = ast_do_while($2, $5, @1.first_line); }   /* missing ';' (reported) */
    | FOR '(' for_init ';' for_condition ';' for_update ')' statement
                                             { $$ = ast_for($3, $5, $7, $9, @1.first_line); }
    | SWITCH '(' expression ')' '{' statement_list '}'
                                             { $$ = ast_switch($3, $6, @1.first_line); }
    | CASE case_value ':'                    { $$ = ast_case($2, 0, @1.first_line); }
    | DEFAULT ':'                            { $$ = ast_case(0, 1, @1.first_line); }
    | BREAK ';'                              { $$ = ast_break(@1.first_line); }
    | CONTINUE ';'                           { $$ = ast_continue(@1.first_line); }
    /* ----- functions (C style, bonus): int main() { ... }, int f(int a) { ... } ----- */
    | type IDENT '(' parameters ')' block    { $$ = ast_function($1, $2, $4, $6->as.block, @2.first_line); }
    | VOID IDENT '(' parameters ')' block    { $$ = ast_function(TYPE_VOID, $2, $4, $6->as.block, @2.first_line); }
    | IDENT '(' call_args ')' ';'            { $$ = ast_call_stmt(ast_call($1, $3, @1.first_line), @1.first_line); }
    | RETURN expression ';'                  { $$ = ast_return($2, @1.first_line); }
    | RETURN ';'                             { $$ = ast_return(NULL, @1.first_line); }
    | PRINTF '(' STRING_LITERAL printf_args ')' ';'
                                             { $$ = ast_printf($3, $4, @1.first_line); }
    | SCANF '(' STRING_LITERAL scanf_args ')' ';'
                                             { $$ = ast_scanf($3, $4, @1.first_line); }
    | error ';'                             { $$ = ast_error(@1.first_line); yyerrok; }
    | error '}'                              { $$ = ast_error(@1.first_line); yyerrok; }
    ;

/* Assignments, usable as statements and in the parts of a for loop. */
simple_statement
    : lvalue '=' expression                  { $$ = ast_assign_to($1, -1, $3, @2.first_line); }
    | lvalue ADD_ASSIGN expression           { $$ = ast_assign_to($1, OP_ADD, $3, @2.first_line); }
    | lvalue SUB_ASSIGN expression           { $$ = ast_assign_to($1, OP_SUB, $3, @2.first_line); }
    | lvalue MUL_ASSIGN expression           { $$ = ast_assign_to($1, OP_MUL, $3, @2.first_line); }
    | lvalue DIV_ASSIGN expression           { $$ = ast_assign_to($1, OP_DIV, $3, @2.first_line); }
    | lvalue MOD_ASSIGN expression           { $$ = ast_assign_to($1, OP_MOD, $3, @2.first_line); }
    | lvalue INC                             { $$ = ast_incdec($1, 1, @2.first_line); }
    | lvalue DEC                             { $$ = ast_incdec($1, 0, @2.first_line); }
    | INC lvalue                             { $$ = ast_incdec($2, 1, @1.first_line); }
    | DEC lvalue                             { $$ = ast_incdec($2, 0, @1.first_line); }
    ;

/* The left side of an assignment: a variable or an array element. */
lvalue
    : IDENT                                  { $$ = ast_name($1, @1.first_line); }
    | IDENT '[' expression ']'               { $$ = ast_index($1, $3, @1.first_line); }
    ;

for_init
    : /* empty */                            { $$ = NULL; }
    | type IDENT '=' expression              { $$ = ast_decl($1, $2, $4, @2.first_line); }
    | simple_statement                       { $$ = $1; }
    ;

for_condition
    : /* empty: always true */               { $$ = NULL; }
    | expression                             { $$ = $1; }
    ;

for_update
    : /* empty */                            { $$ = NULL; }
    | simple_statement                       { $$ = $1; }
    ;

case_value
    : INT_LITERAL                            { $$ = $1; }
    | '-' INT_LITERAL                        { $$ = -$2; }
    ;

/* int a[3] = {1, 2, 3}; */
initializer
    : '{' arg_list '}'                       { $$ = $2; }
    ;

parameters
    : /* empty */                            { $$ = ast_param_list_new(); }
    | VOID                                   { $$ = ast_param_list_new(); }
    | parameter_list                         { $$ = $1; }
    ;

parameter_list
    : type IDENT                             { $$ = ast_param_list_append(NULL, $1, $2, @2.first_line); }
    | parameter_list ',' type IDENT          { $$ = ast_param_list_append($1, $3, $4, @4.first_line); }
    ;

call_args
    : /* empty */                            { $$ = ast_expr_list_new(); }
    | arg_list                               { $$ = $1; }
    ;

arg_list
    : expression                             { $$ = ast_expr_list_append(NULL, $1); }
    | arg_list ',' expression                { $$ = ast_expr_list_append($1, $3); }
    ;

printf_args
    : /* empty */                            { $$ = ast_expr_list_new(); }
    | printf_args ',' expression             { $$ = ast_expr_list_append($1, $3); }
    ;

/* scanf reads into variables, each passed by address: &x */
scanf_args
    : /* empty */                            { $$ = ast_expr_list_new(); }
    | scanf_args ',' '&' IDENT               { $$ = ast_expr_list_append($1, ast_name($4, @4.first_line)); }
    | scanf_args ',' '&' IDENT '[' expression ']'
                                             { $$ = ast_expr_list_append($1, ast_index($4, $6, @4.first_line)); }
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
    | IDENT '[' expression ']'               { $$ = ast_index($1, $3, @1.first_line); }
    | IDENT '(' call_args ')'                { $$ = ast_call($1, $3, @1.first_line); }
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
