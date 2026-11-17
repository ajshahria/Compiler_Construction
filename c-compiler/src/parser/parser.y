%code requires {
#include "ast.h"
}
%{
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
int yylex(void); void yyerror(const char *s); extern int yylineno; extern StmtList *program_root; int parser_errors=0;
%}
%define parse.error detailed
%locations
%union { char *text; long integer; double real; Expr *expr; Stmt *stmt; StmtList *list; Type type; }
%token <text> IDENT
%token <integer> INT_LITERAL
%token <real> FLOAT_LITERAL
%token INT FLOAT BOOL IF ELSE WHILE PRINT TRUE FALSE
%token EQ NE LE GE AND OR
%type <type> type
%type <expr> expression
%type <stmt> statement block
%type <list> statements
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%precedence '!' UMINUS
%precedence IFX
%precedence ELSE
%start program
%%
program: statements { program_root=$1; };
statements: %empty { $$=list_new(); }
         | statements statement { $$=list_add($1,$2); };
statement: type IDENT ';' { $$=stmt_decl($1,$2,NULL,@1.first_line); }
         | type IDENT '=' expression ';' { $$=stmt_decl($1,$2,$4,@1.first_line); }
         | IDENT '=' expression ';' { $$=stmt_assign($1,$3,@1.first_line); }
         | PRINT expression ';' { $$=stmt_print($2,@1.first_line); }
         | IF '(' expression ')' statement %prec IFX { $$=stmt_if($3,$5,NULL,@1.first_line); }
         | IF '(' expression ')' statement ELSE statement { $$=stmt_if($3,$5,$7,@1.first_line); }
         | WHILE '(' expression ')' statement { $$=stmt_while($3,$5,@1.first_line); }
         | block { $$=$1; }
         | error ';' { $$=stmt_error(@1.first_line); yyerrok; }
         ;
block: '{' statements '}' { $$=stmt_block($2,@1.first_line); };
type: INT { $$=TYPE_INT; } | FLOAT { $$=TYPE_FLOAT; } | BOOL { $$=TYPE_BOOL; };
expression: INT_LITERAL { $$=expr_int($1,@1.first_line); }
          | FLOAT_LITERAL { $$=expr_float($1,@1.first_line); }
          | TRUE { $$=expr_bool(1,@1.first_line); }
          | FALSE { $$=expr_bool(0,@1.first_line); }
          | IDENT { $$=expr_name($1,@1.first_line); }
          | '(' expression ')' { $$=$2; }
          | expression '+' expression { $$=expr_binary(OP_ADD,$1,$3,@2.first_line); }
          | expression '-' expression { $$=expr_binary(OP_SUB,$1,$3,@2.first_line); }
          | expression '*' expression { $$=expr_binary(OP_MUL,$1,$3,@2.first_line); }
          | expression '/' expression { $$=expr_binary(OP_DIV,$1,$3,@2.first_line); }
          | expression '%' expression { $$=expr_binary(OP_MOD,$1,$3,@2.first_line); }
          | expression EQ expression { $$=expr_binary(OP_EQ,$1,$3,@2.first_line); }
          | expression NE expression { $$=expr_binary(OP_NE,$1,$3,@2.first_line); }
          | expression '<' expression { $$=expr_binary(OP_LT,$1,$3,@2.first_line); }
          | expression LE expression { $$=expr_binary(OP_LE,$1,$3,@2.first_line); }
          | expression '>' expression { $$=expr_binary(OP_GT,$1,$3,@2.first_line); }
          | expression GE expression { $$=expr_binary(OP_GE,$1,$3,@2.first_line); }
          | expression AND expression { $$=expr_binary(OP_AND,$1,$3,@2.first_line); }
          | expression OR expression { $$=expr_binary(OP_OR,$1,$3,@2.first_line); }
          | '!' expression { $$=expr_unary(OP_NOT,$2,@1.first_line); }
          | '-' expression %prec UMINUS { $$=expr_unary(OP_NEG,$2,@1.first_line); }
          ;
%%
void yyerror(const char *s){fprintf(stderr,"line %d: syntax error: %s\n",yylineno,s);parser_errors++;}

