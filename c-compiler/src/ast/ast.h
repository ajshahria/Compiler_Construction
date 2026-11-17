#ifndef TRICORA_AST_H
#define TRICORA_AST_H
#include <stddef.h>

typedef enum { TYPE_ERROR, TYPE_INT, TYPE_FLOAT, TYPE_BOOL } Type;
typedef enum { EX_INT, EX_FLOAT, EX_BOOL, EX_NAME, EX_BINARY, EX_UNARY } ExprKind;
typedef enum { ST_BLOCK, ST_DECL, ST_ASSIGN, ST_PRINT, ST_IF, ST_WHILE, ST_EXPR, ST_ERROR } StmtKind;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct StmtList { Stmt **items; size_t count, capacity; } StmtList;
struct Expr {
    ExprKind kind; int line; Type inferred;
    union { long integer; double real; int boolean; char *name;
        struct { int op; Expr *left, *right; } binary;
        struct { int op; Expr *child; } unary; } as;
};
struct Stmt {
    StmtKind kind; int line;
    union { struct { Type type; char *name; Expr *init; } decl;
        struct { char *name; Expr *value; } assign;
        Expr *print; Expr *expression;
        struct { Expr *condition; Stmt *yes, *no; } branch;
        struct { Expr *condition; Stmt *body; } loop;
        StmtList *block; } as;
};
enum { OP_ADD=256, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_AND, OP_OR, OP_NOT, OP_NEG };
Expr *expr_int(long v, int line); Expr *expr_float(double v, int line); Expr *expr_bool(int v, int line);
Expr *expr_name(char *name, int line); Expr *expr_binary(int op, Expr *a, Expr *b, int line); Expr *expr_unary(int op, Expr *a, int line);
Stmt *stmt_decl(Type t, char *name, Expr *init, int line); Stmt *stmt_assign(char *name, Expr *v, int line);
Stmt *stmt_print(Expr *e, int line); Stmt *stmt_if(Expr *c, Stmt *a, Stmt *b, int line); Stmt *stmt_while(Expr *c, Stmt *b, int line);
Stmt *stmt_block(StmtList *items, int line); Stmt *stmt_error(int line); StmtList *list_new(void); StmtList *list_add(StmtList *l, Stmt *s);
const char *type_name(Type t); void ast_print(StmtList *items);
#endif

