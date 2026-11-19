/* parser.y - Phase 2: Syntax analysis (Bison). Builds the AST.
 *
 * Grammar (see docs/grammar.md). A program is a list of statements; 'int main() { ... }'
 * is accepted as a statement whose body becomes a block, so both the lab manual's
 * top-level programs and ordinary C++ programs work.
 *
 * Operator precedence (low -> high): ||  &&  == !=  < > <= >=  + -  * / %  ! unary-
 * Dangling else is resolved with %prec: else binds to the nearest if.
 * Error recovery: 'error ;' skips to the end of a bad statement,
 * 'error }' skips to the end of a bad block.
 *
 * Shorthand is rewritten into core nodes right here in the parser:
 *   x += e  ->  x = x + e      x++ / ++x  ->  x = x + 1      for(i;c;s) b  ->  { i; while(c) b step s }
 *   int a, b = 2;  ->  a DeclGroup of single declarations      double/long/short  ->  float/int
 */
%{
#include <cstdio>
#include <cstdlib>
#include <string>
#include "ast.h"
#include "diagnostics.h"

extern int yylex();
extern int yylineno;
extern int g_lastLexErrorLine;
void yyerror(const char* msg);

BlockNode* g_program = NULL;   /* root of the AST, read by main */

/* Facts about 'main' collected while parsing; main.cpp turns them into errors. */
int   g_mainCount = 0;         /* how many main() definitions were found */
int   g_secondMainLine = 0;    /* line of the 2nd definition (redefinition error) */
int   g_mainVoidLine = 0;      /* line of 'void main' (main must return int) */
Node* g_mainBlock = NULL;      /* the body block of the first main() */

static Node* registerMain(Node* body, int line, bool isVoid) {
    g_mainCount++;
    if (g_mainCount == 1) g_mainBlock = body;
    else if (!g_secondMainLine) g_secondMainLine = line;
    if (isVoid && !g_mainVoidLine) g_mainVoidLine = line;
    return body;
}

/* build  name = name <op> rhs  (used by +=, -=, ++ ...) */
static Node* compound(const char* name, const char* op, Node* rhs, int line) {
    return new AssignNode(name, new BinaryNode(op, new IdentNode(name, line), rhs, line), line);
}

/* give every declarator of 'int a, b = 2;' its type; a single declarator stays a plain VarDecl */
static Node* finishDecl(Type t, BlockNode* group) {
    for (size_t i = 0; i < group->stmts.size(); i++)
        static_cast<VarDeclNode*>(group->stmts[i])->declType = t;
    return group->stmts.size() == 1 ? group->stmts[0] : group;
}
%}

%error-verbose
%locations

%code requires {
#include "ast.h"
}

%union {
    long        ival;
    double      fval;
    char*       sval;
    Type        tval;
    Node*       node;
    BlockNode*  block;
    CoutNode*   coutn;
    CinNode*    cinn;
    SwitchNode* swn;
}

%token KW_INT KW_FLOAT KW_BOOL KW_IF KW_ELSE KW_WHILE KW_PRINT KW_TRUE KW_FALSE
%token KW_FOR KW_DO KW_SWITCH KW_CASE KW_DEFAULT KW_BREAK KW_CONTINUE KW_RETURN
%token KW_MAIN KW_VOID KW_CONST KW_DOUBLE KW_LONG KW_SHORT
%token KW_COUT KW_CIN KW_ENDL SHL SHR INC DEC ADDEQ SUBEQ MULEQ DIVEQ
%token <ival> INT_LIT
%token <fval> FLOAT_LIT
%token <sval> IDENTIFIER STRING_LIT
%token LE GE EQ NE AND OR

%type <tval>  type
%type <ival>  case_value
%type <node>  stmt decl declarator assign_stmt simple_assign if_stmt while_stmt do_stmt for_stmt
%type <node>  print_stmt return_stmt main_def expr block
%type <node>  for_init for_cond for_step
%type <block> stmt_list program declarators
%type <coutn> cout_stmt cout_list
%type <cinn>  cin_stmt cin_list
%type <swn>   switch_stmt case_list

%nonassoc LOWER_THAN_ELSE
%nonassoc KW_ELSE
%right '?' ':'
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS

%%

program
    : stmt_list                 { g_program = $1; $$ = $1; }
    ;

stmt_list
    : /* empty */               { $$ = new BlockNode(NodeKind::Program, yylineno); }
    | stmt_list stmt            { if ($2) $1->stmts.push_back($2); $$ = $1; }
    ;

stmt
    : decl
    | assign_stmt
    | if_stmt
    | while_stmt
    | do_stmt
    | for_stmt
    | print_stmt
    | return_stmt
    | main_def
    | block
    | cout_stmt                 { $$ = $1; }
    | cin_stmt                  { $$ = $1; }
    | switch_stmt               { $$ = $1; }
    | KW_BREAK ';'              { $$ = new JumpNode(NodeKind::Break, @1.first_line); }
    | KW_CONTINUE ';'           { $$ = new JumpNode(NodeKind::Continue, @1.first_line); }
    | ';'                       { $$ = new BlockNode(NodeKind::Block, @1.first_line); }
    | error ';'                 { yyerrok; $$ = NULL; }
    ;

block
    : '{' stmt_list '}'         { $2->kind = NodeKind::Block; $$ = $2; }
    | '{' stmt_list error '}'   { yyerrok; $2->kind = NodeKind::Block; $$ = $2; }
    ;

/* int main() { ... }  -- the body is an ordinary block */
main_def
    : KW_INT KW_MAIN '(' ')' block              { $$ = registerMain($5, @1.first_line, false); }
    | KW_INT KW_MAIN '(' KW_VOID ')' block      { $$ = registerMain($6, @1.first_line, false); }
    | KW_VOID KW_MAIN '(' ')' block             { $$ = registerMain($5, @1.first_line, true); }
    | KW_MAIN '(' ')' block
        { reportError(PH_SYNTAX, @1.first_line, "'main' has no return type: write 'int main()'");
          $$ = registerMain($4, @1.first_line, false); }
    ;

type
    : KW_INT                    { $$ = TY_INT; }
    | KW_FLOAT                  { $$ = TY_FLOAT; }
    | KW_DOUBLE                 { $$ = TY_FLOAT; }
    | KW_BOOL                   { $$ = TY_BOOL; }
    | KW_LONG                   { $$ = TY_INT; }
    | KW_LONG KW_LONG           { $$ = TY_INT; }
    | KW_SHORT                  { $$ = TY_INT; }
    ;

decl
    : type declarators ';'              { $$ = finishDecl($1, $2); }
    | KW_CONST type declarators ';'     { $$ = finishDecl($2, $3); }
    ;

declarators
    : declarator                { $$ = new BlockNode(NodeKind::DeclGroup, @1.first_line); $$->stmts.push_back($1); }
    | declarators ',' declarator { $1->stmts.push_back($3); $$ = $1; }
    ;

declarator
    : IDENTIFIER                { $$ = new VarDeclNode(TY_ERROR, $1, NULL, @1.first_line); free($1); }
    | IDENTIFIER '=' expr       { $$ = new VarDeclNode(TY_ERROR, $1, $3, @1.first_line); free($1); }
    ;

assign_stmt
    : simple_assign ';'         { $$ = $1; }
    ;

/* an assignment without its ';' (also used inside for(...)) */
simple_assign
    : IDENTIFIER '=' expr       { $$ = new AssignNode($1, $3, @1.first_line); free($1); }
    | IDENTIFIER ADDEQ expr     { $$ = compound($1, "+", $3, @1.first_line); free($1); }
    | IDENTIFIER SUBEQ expr     { $$ = compound($1, "-", $3, @1.first_line); free($1); }
    | IDENTIFIER MULEQ expr     { $$ = compound($1, "*", $3, @1.first_line); free($1); }
    | IDENTIFIER DIVEQ expr     { $$ = compound($1, "/", $3, @1.first_line); free($1); }
    | IDENTIFIER INC            { $$ = compound($1, "+", new IntLitNode(1, @1.first_line), @1.first_line); free($1); }
    | IDENTIFIER DEC            { $$ = compound($1, "-", new IntLitNode(1, @1.first_line), @1.first_line); free($1); }
    | INC IDENTIFIER            { $$ = compound($2, "+", new IntLitNode(1, @1.first_line), @1.first_line); free($2); }
    | DEC IDENTIFIER            { $$ = compound($2, "-", new IntLitNode(1, @1.first_line), @1.first_line); free($2); }
    ;

if_stmt
    : KW_IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                { $$ = new IfNode($3, $5, NULL, @1.first_line); }
    | KW_IF '(' expr ')' stmt KW_ELSE stmt
                                { $$ = new IfNode($3, $5, $7, @1.first_line); }
    ;

while_stmt
    : KW_WHILE '(' expr ')' stmt  { $$ = new WhileNode($3, $5, @1.first_line); }
    ;

do_stmt
    : KW_DO stmt KW_WHILE '(' expr ')' ';'  { $$ = new DoWhileNode($2, $5, @1.first_line); }
    ;

/* for (init; cond; step) body  ==>  { init; while (cond) body  [step runs after body / on continue] } */
for_stmt
    : KW_FOR '(' for_init ';' for_cond ';' for_step ')' stmt
        {
            BlockNode* outer = new BlockNode(NodeKind::Block, @1.first_line);
            if ($3) outer->stmts.push_back($3);
            outer->stmts.push_back(new WhileNode($5, $9, @1.first_line, $7));
            $$ = outer;
        }
    ;

for_init
    : /* empty */                   { $$ = NULL; }
    | simple_assign                 { $$ = $1; }
    | type IDENTIFIER '=' expr      { $$ = new VarDeclNode($1, $2, $4, @1.first_line); free($2); }
    ;

for_cond
    : /* empty */                   { $$ = new BoolLitNode(true, yylineno); }
    | expr                          { $$ = $1; }
    ;

for_step
    : /* empty */                   { $$ = NULL; }
    | simple_assign                 { $$ = $1; }
    ;

switch_stmt
    : KW_SWITCH '(' expr ')' '{' case_list '}'
        { $6->expr = $3; $6->line = @1.first_line; $$ = $6; }
    ;

case_list
    : /* empty */               { $$ = new SwitchNode(NULL, yylineno); }
    | case_list KW_CASE case_value ':' stmt_list
        {
            SwitchCase c; c.isDefault = false; c.value = $3; c.line = @2.first_line;
            $5->kind = NodeKind::DeclGroup; c.body = $5;
            $1->cases.push_back(c); $$ = $1;
        }
    | case_list KW_DEFAULT ':' stmt_list
        {
            SwitchCase c; c.isDefault = true; c.value = 0; c.line = @2.first_line;
            $4->kind = NodeKind::DeclGroup; c.body = $4;
            $1->cases.push_back(c); $$ = $1;
        }
    ;

case_value
    : INT_LIT                   { $$ = $1; }
    | '-' INT_LIT               { $$ = -$2; }
    ;

print_stmt
    : KW_PRINT expr ';'         { $$ = new PrintNode($2, @1.first_line); }
    ;

return_stmt
    : KW_RETURN ';'             { $$ = new ReturnNode(NULL, @1.first_line); }
    | KW_RETURN expr ';'        { $$ = new ReturnNode($2, @1.first_line); }
    ;

/* cout << a << "text" << endl;  -- one item per << */
cout_stmt
    : KW_COUT SHL cout_list ';' { $3->line = @1.first_line; $$ = $3; }
    ;

cout_list
    : expr
        { $$ = new CoutNode(@1.first_line);
          CoutItem it; it.kind = 0; it.expr = $1; $$->items.push_back(it); }
    | STRING_LIT
        { $$ = new CoutNode(@1.first_line);
          CoutItem it; it.kind = 1; it.expr = NULL; it.text = $1; $$->items.push_back(it); free($1); }
    | KW_ENDL
        { $$ = new CoutNode(@1.first_line);
          CoutItem it; it.kind = 2; it.expr = NULL; $$->items.push_back(it); }
    | '(' expr '?' STRING_LIT ':' STRING_LIT ')'
        { $$ = new CoutNode(@1.first_line);
          CoutItem it; it.kind = 3; it.expr = $2; it.text = $4; it.text2 = $6;
          $$->items.push_back(it); free($4); free($6); }
    | cout_list SHL '(' expr '?' STRING_LIT ':' STRING_LIT ')'
        { CoutItem it; it.kind = 3; it.expr = $4; it.text = $6; it.text2 = $8;
          $1->items.push_back(it); free($6); free($8); $$ = $1; }
    | cout_list SHL expr
        { CoutItem it; it.kind = 0; it.expr = $3; $1->items.push_back(it); $$ = $1; }
    | cout_list SHL STRING_LIT
        { CoutItem it; it.kind = 1; it.expr = NULL; it.text = $3; $1->items.push_back(it); free($3); $$ = $1; }
    | cout_list SHL KW_ENDL
        { CoutItem it; it.kind = 2; it.expr = NULL; $1->items.push_back(it); $$ = $1; }
    ;

/* cin >> a >> b; */
cin_stmt
    : KW_CIN SHR cin_list ';'   { $3->line = @1.first_line; $$ = $3; }
    ;

cin_list
    : IDENTIFIER                { $$ = new CinNode(@1.first_line); $$->vars.push_back(new IdentNode($1, @1.first_line)); free($1); }
    | cin_list SHR IDENTIFIER   { $1->vars.push_back(new IdentNode($3, @3.first_line)); free($3); $$ = $1; }
    ;

expr
    : expr '?' expr ':' expr    { $$ = new TernaryNode($1, $3, $5, @2.first_line); }
    | expr OR expr              { $$ = new BinaryNode("||", $1, $3, @2.first_line); }
    | expr AND expr             { $$ = new BinaryNode("&&", $1, $3, @2.first_line); }
    | expr EQ expr              { $$ = new BinaryNode("==", $1, $3, @2.first_line); }
    | expr NE expr              { $$ = new BinaryNode("!=", $1, $3, @2.first_line); }
    | expr '<' expr             { $$ = new BinaryNode("<", $1, $3, @2.first_line); }
    | expr '>' expr             { $$ = new BinaryNode(">", $1, $3, @2.first_line); }
    | expr LE expr              { $$ = new BinaryNode("<=", $1, $3, @2.first_line); }
    | expr GE expr              { $$ = new BinaryNode(">=", $1, $3, @2.first_line); }
    | expr '+' expr             { $$ = new BinaryNode("+", $1, $3, @2.first_line); }
    | expr '-' expr             { $$ = new BinaryNode("-", $1, $3, @2.first_line); }
    | expr '*' expr             { $$ = new BinaryNode("*", $1, $3, @2.first_line); }
    | expr '/' expr             { $$ = new BinaryNode("/", $1, $3, @2.first_line); }
    | expr '%' expr             { $$ = new BinaryNode("%", $1, $3, @2.first_line); }
    | '!' expr                  { $$ = new UnaryNode("!", $2, @1.first_line); }
    | '-' expr %prec UMINUS     { $$ = new UnaryNode("-", $2, @1.first_line); }
    | '(' expr ')'              { $$ = $2; }
    | INT_LIT                   { $$ = new IntLitNode($1, @1.first_line); }
    | FLOAT_LIT                 { $$ = new FloatLitNode($1, @1.first_line); }
    | KW_TRUE                   { $$ = new BoolLitNode(true, @1.first_line); }
    | KW_FALSE                  { $$ = new BoolLitNode(false, @1.first_line); }
    | IDENTIFIER                { $$ = new IdentNode($1, @1.first_line); free($1); }
    ;

%%

void yyerror(const char* msg) {
    /* a bad token on this line was already reported by the lexer: don't pile on */
    if (g_lastLexErrorLine == yylineno) return;
    reportError(PH_SYNTAX, yylineno, msg);
}
