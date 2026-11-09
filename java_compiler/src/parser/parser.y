%{
#include "ast/ast.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern int yylex(); extern int yylineno;
void yyerror(const char*);
Node* syntaxRoot=nullptr;
static Node* append(Node*a,Node*b){if(!a)a=makeNode("block","",yylineno);if(b)a->children.push_back(b);return a;}
%}
%locations
%union { char* str; struct Node* node; }
%token <str> IDENT NUMBER
%token CLASS PUBLIC STATIC VOID MAIN STRING INT FLOAT BOOLEAN IF ELSE WHILE TRUE FALSE PRINT
%token EQ NE LE GE AND OR
%type <node> program block statements statement expression
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%%
program: CLASS IDENT '{' PUBLIC STATIC VOID MAIN '(' STRING '[' ']' IDENT ')' block '}' { syntaxRoot=makeNode("program",$2,@1.first_line,$14); free($2); free($12); }
;
block: '{' statements '}' { $$=$2?$2:makeNode("block","",@1.first_line); }
;
statements: { $$=makeNode("block","",@$.first_line); }
 | statements statement { $$=append($1,$2); }
;
statement: block { $$=$1; }
 | INT IDENT ';' { $$=makeNode("decl","int",@1.first_line,makeNode("id",$2,@2.first_line)); free($2); }
 | FLOAT IDENT ';' { $$=makeNode("decl","float",@1.first_line,makeNode("id",$2,@2.first_line)); free($2); }
 | BOOLEAN IDENT ';' { $$=makeNode("decl","boolean",@1.first_line,makeNode("id",$2,@2.first_line)); free($2); }
 | INT IDENT '=' expression ';' { $$=makeNode("decl","int",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
 | FLOAT IDENT '=' expression ';' { $$=makeNode("decl","float",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
 | BOOLEAN IDENT '=' expression ';' { $$=makeNode("decl","boolean",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
 | IDENT '=' expression ';' { $$=makeNode("assign",$1,@1.first_line,$3); free($1); }
 | PRINT '(' expression ')' ';' { $$=makeNode("print","",@1.first_line,$3); }
 | IF '(' expression ')' statement %prec LOWER_THAN_ELSE { $$=makeNode("if","",@1.first_line,$3,$5); }
 | IF '(' expression ')' statement ELSE statement { $$=makeNode("if","",@1.first_line,$3,$5,$7); }
 | WHILE '(' expression ')' statement { $$=makeNode("while","",@1.first_line,$3,$5); }
;
expression: NUMBER { $$=makeNode("literal",$1,@1.first_line); free($1); }
 | TRUE { $$=makeNode("literal","true",@1.first_line); }
 | FALSE { $$=makeNode("literal","false",@1.first_line); }
 | IDENT { $$=makeNode("id",$1,@1.first_line); free($1); }
 | '(' expression ')' { $$=$2; }
 | '-' expression %prec UMINUS { $$=makeNode("unary","-",@1.first_line,$2); }
 | '!' expression { $$=makeNode("unary","!",@1.first_line,$2); }
 | expression '+' expression { $$=makeNode("binary","+",@2.first_line,$1,$3); }
 | expression '-' expression { $$=makeNode("binary","-",@2.first_line,$1,$3); }
 | expression '*' expression { $$=makeNode("binary","*",@2.first_line,$1,$3); }
 | expression '/' expression { $$=makeNode("binary","/",@2.first_line,$1,$3); }
 | expression '%' expression { $$=makeNode("binary","%",@2.first_line,$1,$3); }
 | expression '<' expression { $$=makeNode("binary","<",@2.first_line,$1,$3); }
 | expression '>' expression { $$=makeNode("binary",">",@2.first_line,$1,$3); }
 | expression LE expression { $$=makeNode("binary","<=",@2.first_line,$1,$3); }
 | expression GE expression { $$=makeNode("binary",">=",@2.first_line,$1,$3); }
 | expression EQ expression { $$=makeNode("binary","==",@2.first_line,$1,$3); }
 | expression NE expression { $$=makeNode("binary","!=",@2.first_line,$1,$3); }
 | expression AND expression { $$=makeNode("binary","&&",@2.first_line,$1,$3); }
 | expression OR expression { $$=makeNode("binary","||",@2.first_line,$1,$3); }
;
%%
void yyerror(const char*s){fprintf(stderr,"line %d: syntax error: %s\n",yylineno,s);}
