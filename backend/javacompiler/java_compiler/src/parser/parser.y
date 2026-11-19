%{
#include "ast/ast.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern int yylex(); extern int yylineno;
void yyerror(const char*);
Node* syntaxRoot=nullptr;
static Node* append(Node*a,Node*b){if(!a)a=makeNode("block","",yylineno);if(b)a->children.push_back(b);return a;}
static Node* slots(const char*k,const char*t,int l,std::initializer_list<Node*> c){Node*n=makeNode(k,t,l);n->children.assign(c.begin(),c.end());return n;}
%}
%locations
%union { char* str; struct Node* node; }
%token <str> IDENT NUMBER
%token CLASS PUBLIC STATIC VOID MAIN STRING INT FLOAT BOOLEAN IF ELSE WHILE FOR DO SWITCH CASE DEFAULT BREAK TRUE FALSE PRINT
%token EQ NE LE GE AND OR INC DEC
%token INVALID
%type <node> program block statements statement controlled_statement expression for_init for_update maybe_expression switch_cases case_clause
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

statement: controlled_statement { $$=$1; }
 | INVALID { YYABORT; }
 | INT IDENT ';' {
     $$=makeNode("decl","int",@1.first_line,
         makeNode("id",$2,@2.first_line));
     free($2);
 }
 | INT IDENT error { fprintf(stderr,"line %d: syntax error: expected ';' after declaration\n",@1.first_line); YYABORT; }
 | FLOAT IDENT ';' {
     $$=makeNode("decl","float",@1.first_line,
         makeNode("id",$2,@2.first_line));
     free($2);
 }
 | FLOAT IDENT error { fprintf(stderr,"line %d: syntax error: expected ';' after declaration\n",@1.first_line); YYABORT; }
 | BOOLEAN IDENT ';' {
     $$=makeNode("decl","boolean",@1.first_line,
         makeNode("id",$2,@2.first_line));
     free($2);
 }
 | BOOLEAN IDENT error { fprintf(stderr,"line %d: syntax error: expected ';' after declaration\n",@1.first_line); YYABORT; }
 | INT IDENT '=' expression ';' {
     $$=makeNode("decl","int",@1.first_line,
         makeNode("id",$2,@2.first_line),$4);
     free($2);
 }
 | FLOAT IDENT '=' expression ';' {
     $$=makeNode("decl","float",@1.first_line,
         makeNode("id",$2,@2.first_line),$4);
     free($2);
 }
 | BOOLEAN IDENT '=' expression ';' {
     $$=makeNode("decl","boolean",@1.first_line,
         makeNode("id",$2,@2.first_line),$4);
     free($2);
 }
 | FOR '(' for_init ';' maybe_expression ';' for_update ')' controlled_statement { $$=slots("for","",@1.first_line,{$3,$5,$7,$9}); }
 | DO controlled_statement WHILE '(' expression ')' ';' { $$=makeNode("do_while","",@1.first_line,$2,$5); }
 | SWITCH '(' expression ')' '{' switch_cases '}' { $$=makeNode("switch","",@1.first_line,$3,$6); }
;

for_init: { $$=nullptr; }
 | IDENT '=' expression { $$=makeNode("assign",$1,@1.first_line,$3); free($1); }
 | INT IDENT '=' expression { $$=makeNode("decl","int",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
 | FLOAT IDENT '=' expression { $$=makeNode("decl","float",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
 | BOOLEAN IDENT '=' expression { $$=makeNode("decl","boolean",@1.first_line,makeNode("id",$2,@2.first_line),$4); free($2); }
;
for_update: { $$=nullptr; }
 | IDENT '=' expression { $$=makeNode("assign",$1,@1.first_line,$3); free($1); }
 | IDENT INC { $$=makeNode("inc",$1,@1.first_line); free($1); }
 | IDENT DEC { $$=makeNode("dec",$1,@1.first_line); free($1); };
maybe_expression: { $$=nullptr; } | expression { $$=$1; };
switch_cases: { $$=makeNode("cases","",@$.first_line); } | switch_cases case_clause { $$=append($1,$2); };
case_clause: CASE NUMBER ':' statements { $$=makeNode("case","number",@1.first_line,makeNode("literal",$2,@2.first_line),$4); free($2); }
 | CASE TRUE ':' statements { $$=makeNode("case","boolean",@1.first_line,makeNode("literal","true",@2.first_line),$4); }
 | CASE FALSE ':' statements { $$=makeNode("case","boolean",@1.first_line,makeNode("literal","false",@2.first_line),$4); }
 | DEFAULT ':' statements { $$=makeNode("default","",@1.first_line,$3); };

controlled_statement: block { $$=$1; }
 | IDENT '=' expression ';' {
     $$=makeNode("assign",$1,@1.first_line,$3);
     free($1);
 }
 | PRINT '(' expression ')' ';' {
     $$=makeNode("print","",@1.first_line,$3);
 }
 | IF '(' expression ')' controlled_statement %prec LOWER_THAN_ELSE {
     $$=makeNode("if","",@1.first_line,$3,$5);
 }
 | IF '(' expression ')' controlled_statement ELSE controlled_statement {
     $$=makeNode("if","",@1.first_line,$3,$5,$7);
 }
 | WHILE '(' expression ')' controlled_statement {
     $$=makeNode("while","",@1.first_line,$3,$5);
 }
 | BREAK ';' { $$=makeNode("break","",@1.first_line); }
;

expression: NUMBER { $$=makeNode("literal",$1,@1.first_line); free($1); }
 | INVALID { YYABORT; }
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
extern YYLTYPE yylloc;
void yyerror(const char*s){fprintf(stderr,"line %d: syntax error: %s\n",yylloc.first_line,s);}
