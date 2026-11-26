/*
 * parser.y - Bison grammar of MiniJava.
 *
 * A program is the statements of the lab manual's language (section 5)
 * wrapped in a Java class with a main method:
 *         class Name { public static void main(String[] args) { ... } }
 * A bare list of statements, exactly as in the manual's sample program
 * (section 5.5), is accepted too: that is the required language of the lab
 * manual. With --strict only the class form is accepted.
 *
 * Both `print x;` and `System.out.println(x);` print a value, and both
 * `bool` and `boolean` name the boolean type. for, do-while, switch and ++/--
 * are bonus features (manual section 14), and so are continue, arrays
 * (int[] a = new int[5]; int[] b = {1, 2}; a[i], a.length) and static
 * methods with parameters, return values and calls. A class holds main and
 * any number of `static` methods; in the bare form, methods may be written
 * among the statements.
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
#include <string>
#include <vector>

extern int yylex();
void yyerror(const char* message);

Node* syntaxRoot = nullptr;   // the finished AST
int syntaxErrors = 0;         // number of syntax errors reported
bool manualMode = true;       // accept the lab manual's bare list of statements (--strict turns it off)

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

/* Builds a method node: method(name) -> params, body. Takes ownership of name. */
static Node* method(const char* returnType, char* name, Node* params, Node* body, int line) {
    Node* n = makeNode("method", name, line, params, body);
    n->type = returnType;
    std::free(name);
    return n;
}

/* class Name { members }: program -> main body, then the other methods. */
static Node* classProgram(char* name, Node* members, int line) {
    Node* mainBody = nullptr;
    std::vector<Node*> methods;
    for (Node* m : members->children) {
        if (m->kind == "main" && mainBody) {
            std::fprintf(stderr, "line %d: syntax error: class %s has more than one main method\n", m->line, name);
            ++syntaxErrors;
        } else if (m->kind == "main") {
            mainBody = m->children.at(0);
        } else {
            methods.push_back(m);
        }
    }
    if (!mainBody) {
        std::fprintf(stderr, "line %d: syntax error: class %s has no "
                     "'public static void main(String[] args)' method\n", line, name);
        ++syntaxErrors;
        mainBody = makeNode("block", "", line);
    }
    Node* program = makeNode("program", name, line, mainBody);
    for (Node* m : methods) program->children.push_back(m);
    std::free(name);
    return program;
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
%token <str> SCANNER_READ          /* sc.nextInt, sc.nextFloat, ... (the text of the call) */
%token SCANNER NEW SYSTEM_IN       /* Scanner sc = new Scanner(System.in); */
%token CLASS PUBLIC STATIC VOID MAIN STRING
%token INT FLOAT BOOLEAN
%token IF ELSE WHILE FOR DO SWITCH CASE DEFAULT BREAK PRINT PRINTLN
%token TRUE FALSE
%token EQ NE LE GE AND OR INC DEC
%token CONTINUE RETURN             /* bonus */
%token <str> LENGTH                /* a.length (the text is the array name) */

%type <type> type result_type
%type <node> block statements statement expression
%type <node> for_init for_update optional_expression switch_cases case_clause
%type <node> class_members class_member method_definition parameters parameter_list
%type <node> arguments argument_list

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
    : statements
        {
            if (!manualMode) {
                std::fprintf(stderr, "line %d: syntax error: a program must be "
                             "'class Name { public static void main(String[] args) { ... } }' "
                             "(the lab manual's form without a class is rejected because of --strict)\n", $1->line);
                ++syntaxErrors;
            }
            syntaxRoot = makeNode("program", "", 1, $1);
        }
    | CLASS IDENT '{' class_members '}'
        { syntaxRoot = classProgram($2, $4, @1.first_line); delete $4; }
    | PUBLIC CLASS IDENT '{' class_members '}'
        { syntaxRoot = classProgram($3, $5, @1.first_line); delete $5; }
    ;

class_members
    : /* empty */                            { $$ = makeNode("members", "", yylloc.first_line); }
    | class_members class_member             { $$ = append($1, $2); }
    ;

class_member
    : modifiers VOID MAIN '(' STRING '[' ']' IDENT ')' block
                                             { $$ = makeNode("main", "", @3.first_line, $10); std::free($8); }
    | method_definition                      { $$ = $1; }
    ;

/* public static / static (bonus: methods other than main) */
modifiers
    : STATIC
    | PUBLIC STATIC
    ;

method_definition
    : modifiers result_type IDENT '(' parameters ')' block
                                             { $$ = method($2, $3, $5, $7, @3.first_line); }
    ;

result_type
    : type                                   { $$ = $1; }
    | VOID                                   { $$ = "void"; }
    ;

parameters
    : /* empty */                            { $$ = makeNode("params", "", yylloc.first_line); }
    | parameter_list                         { $$ = $1; }
    ;

parameter_list
    : type IDENT                             { $$ = makeNode("params", "", @1.first_line, declaration($1, $2, nullptr, @2.first_line)); $$->children[0]->kind = "param"; }
    | parameter_list ',' type IDENT          { Node* p = declaration($3, $4, nullptr, @4.first_line); p->kind = "param"; $$ = append($1, p); }
    | type '[' ']' IDENT                     { $$ = makeNode("params", "", @1.first_line, declaration((std::string($1) + "[]").c_str(), $4, nullptr, @4.first_line)); $$->children[0]->kind = "param"; }
    | parameter_list ',' type '[' ']' IDENT  { Node* p = declaration((std::string($3) + "[]").c_str(), $6, nullptr, @6.first_line); p->kind = "param"; $$ = append($1, p); }
    ;

arguments
    : /* empty */                            { $$ = makeNode("args", "", yylloc.first_line); }
    | argument_list                          { $$ = $1; }
    ;

argument_list
    : expression                             { $$ = makeNode("args", "", @1.first_line, $1); }
    | argument_list ',' expression           { $$ = append($1, $3); }
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
    /* ----- arrays (bonus) ----- */
    | type '[' ']' IDENT '=' NEW type '[' expression ']' ';'
        {
            $$ = makeNode("array_decl", $1, @4.first_line, makeNode("id", $4, @4.first_line),
                          makeNode("new_array", $7, @6.first_line, $9));
            std::free($4);
        }
    | type '[' ']' IDENT '=' '{' argument_list '}' ';'
        {
            $7->kind = "array_init";
            $$ = makeNode("array_decl", $1, @4.first_line, makeNode("id", $4, @4.first_line), $7);
            std::free($4);
        }
    | IDENT '[' expression ']' '=' expression ';'
                                             { $$ = makeNode("assign_index", $1, @1.first_line, $3, $6); std::free($1); }
    | IDENT '[' expression ']' INC ';'       { $$ = makeNode("inc", $1, @1.first_line, $3); std::free($1); }
    | IDENT '[' expression ']' DEC ';'       { $$ = makeNode("dec", $1, @1.first_line, $3); std::free($1); }
    /* ----- methods (bonus) ----- */
    | STATIC result_type IDENT '(' parameters ')' block      /* bare form: static int f(...) { ... } */
                                             { $$ = method($2, $3, $5, $7, @3.first_line); }
    | IDENT '(' arguments ')' ';'
        { Node* call = makeNode("call", $1, @1.first_line); call->children = $3->children; delete $3;
          $$ = makeNode("call_stmt", "", @1.first_line, call); std::free($1); }
    | RETURN expression ';'                  { $$ = makeNode("return", "", @1.first_line, $2); }
    | RETURN ';'                             { $$ = makeNode("return", "", @1.first_line); }
    | CONTINUE ';'                           { $$ = makeNode("continue", "", @1.first_line); }
    | PRINT expression ';'                   { $$ = makeNode("print", "", @1.first_line, $2); }
    | PRINTLN '(' expression ')' ';'         { $$ = makeNode("print", "", @1.first_line, $3); }
    | SCANNER IDENT '=' NEW SCANNER '(' SYSTEM_IN ')' ';'
                                             { $$ = makeNode("scanner", $2, @2.first_line); std::free($2); }
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
    | SCANNER_READ '(' ')'                   { $$ = makeNode("read", $1, @1.first_line); std::free($1); }
    | IDENT '[' expression ']'               { $$ = makeNode("index", $1, @1.first_line, $3); std::free($1); }
    | LENGTH                                 { $$ = makeNode("length", $1, @1.first_line); std::free($1); }
    | IDENT '(' arguments ')'
        { $$ = makeNode("call", $1, @1.first_line); $$->children = $3->children; delete $3; std::free($1); }
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
 * reported instead. Bison 2.x calls the end of input "$end" and Bison 3.x
 * "end of file"; both are printed as "end of file" so the messages do not
 * depend on the Bison version. */
void yyerror(const char* rawMessage) {
    extern int previousTokenLine, previousToken;
    const char* prefix = "syntax error, ";
    bool previousLineComplete = previousToken == ';' || previousToken == '{' || previousToken == '}';

    if (std::strncmp(rawMessage, prefix, std::strlen(prefix)) == 0)
        rawMessage += std::strlen(prefix);

    std::string text = rawMessage;
    for (std::size_t at; (at = text.find("$end")) != std::string::npos; )
        text.replace(at, 4, "end of file");
    const char* message = text.c_str();

    if (previousToken != 0 && yylloc.first_line > previousTokenLine && !previousLineComplete)
        std::fprintf(stderr, "line %d: syntax error: %s on line %d (missing ';' at the end of line %d?)\n",
                     previousTokenLine, message, yylloc.first_line, previousTokenLine);
    else
        std::fprintf(stderr, "line %d: syntax error: %s\n", yylloc.first_line, message);
    ++syntaxErrors;
}
