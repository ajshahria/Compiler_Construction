/*
 * ast.h - Abstract Syntax Tree for the Tricora mini language.
 *
 * The parser builds this tree; the semantic analyzer annotates it with types
 * and the TAC generator walks it to emit three-address code. Parentheses and
 * semicolons are not stored: only meaningful constructs become nodes.
 */
#ifndef TRICORA_AST_H
#define TRICORA_AST_H

#include <stddef.h>
#include <stdio.h>

/* Data types of the language. TYPE_ERROR marks an expression whose type
 * could not be determined (so one mistake does not cause a flood of errors). */
typedef enum { TYPE_ERROR, TYPE_INT, TYPE_FLOAT, TYPE_BOOL } Type;

/* Operators stored in BinaryOp / UnaryOp nodes. */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,     /* arithmetic  */
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,   /* relational  */
    OP_AND, OP_OR, OP_NOT,                      /* logical     */
    OP_NEG                                      /* unary minus */
} Operator;

/* ---------- expressions ---------- */

typedef enum {
    EX_INT,      /* IntLiteral   */
    EX_FLOAT,    /* FloatLiteral */
    EX_BOOL,     /* BoolLiteral  */
    EX_NAME,     /* Identifier   */
    EX_BINARY,   /* BinaryOp     */
    EX_UNARY     /* UnaryOp      */
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind kind;
    int line;
    Type type;                 /* filled in by the semantic analyzer */
    char *tac_name;            /* Identifier: unique name chosen by the semantic analyzer */
    union {
        long integer;
        double real;
        int boolean;
        char *name;
        struct { Operator op; Expr *left, *right; } binary;
        struct { Operator op; Expr *operand; } unary;
    } as;
};

/* ---------- statements ---------- */

typedef enum {
    ST_DECL,     /* VarDecl  : int x;  or  int x = e;  */
    ST_ASSIGN,   /* Assign   : x = e;                  */
    ST_PRINT,    /* Print    : print e;                */
    ST_IF,       /* If / IfElse                        */
    ST_WHILE,    /* While                              */
    ST_BLOCK,    /* Block    : { ... } (new scope)     */
    ST_FUNCTION, /* Function : int main() { ... }   (C style, bonus) */
    ST_RETURN,   /* Return   : return e;            (C style, bonus) */
    ST_PRINTF,   /* Printf   : printf("...", e, ...); (C style, bonus) */
    ST_ERROR     /* placeholder left by syntax-error recovery */
} StmtKind;

typedef struct Stmt Stmt;

/* Growable list of statements (program body or block body). */
typedef struct {
    Stmt **items;
    size_t count, capacity;
} StmtList;

/* Growable list of expressions (the arguments of printf). */
typedef struct {
    Expr **items;
    size_t count, capacity;
} ExprList;

struct Stmt {
    StmtKind kind;
    int line;
    union {
        struct { Type type; char *name; Expr *init; char *tac_name; } decl;
        struct { char *name; Expr *value; char *tac_name; } assign;
        Expr *print;
        struct { Expr *cond; Stmt *then_branch, *else_branch; } if_stmt;
        struct { Expr *cond; Stmt *body; } while_stmt;
        StmtList *block;
        struct { Type type; char *name; StmtList *body; } function;
        Expr *ret;                                    /* NULL for `return;` */
        struct { char *format; ExprList *args; } printf_stmt;   /* format: text between the quotes, escapes kept */
    } as;
};

/* Constructors used by the parser actions. */
Expr *ast_int(long value, int line);
Expr *ast_float(double value, int line);
Expr *ast_bool(int value, int line);
Expr *ast_name(char *name, int line);
Expr *ast_binary(Operator op, Expr *left, Expr *right, int line);
Expr *ast_unary(Operator op, Expr *operand, int line);

Stmt *ast_decl(Type type, char *name, Expr *init, int line);
Stmt *ast_assign(char *name, Expr *value, int line);
Stmt *ast_print(Expr *value, int line);
Stmt *ast_if(Expr *cond, Stmt *then_branch, Stmt *else_branch, int line);
Stmt *ast_while(Expr *cond, Stmt *body, int line);
Stmt *ast_block(StmtList *body, int line);
Stmt *ast_error(int line);
Stmt *ast_function(Type type, char *name, StmtList *body, int line);
Stmt *ast_return(Expr *value, int line);
Stmt *ast_printf(char *format, ExprList *args, int line);

StmtList *ast_list_new(void);
StmtList *ast_list_append(StmtList *list, Stmt *stmt);
ExprList *ast_expr_list_new(void);
ExprList *ast_expr_list_append(ExprList *list, Expr *expr);

/* Helpers. */
const char *type_name(Type type);
const char *operator_symbol(Operator op);

/* #include headers seen by the lexer (C style). */
int header_included(const char *name);

/* One piece of a printf format string: literal text (still escaped, exactly
 * as written between the quotes) or a conversion such as %d or %.2f. */
typedef struct {
    int is_conversion;
    char conversion;        /* 'd', 'i', 'f' or 'g' (or the bad character) */
    int precision;          /* digits after %. ; -1 when not given */
    const char *text;       /* literal text: start and length inside the format */
    size_t length;
} FormatPiece;

/* Reads the next piece of a printf format starting at *pos.
 * Returns 1 for a piece, 0 at the end, -1 for an unsupported conversion
 * (piece->conversion then holds the offending character, or 0 at the end). */
int format_next(const char *format, size_t *pos, FormatPiece *piece);

/* Prints the tree with indentation (the --ast option). */
void ast_print_program(StmtList *program, FILE *out);

#endif
