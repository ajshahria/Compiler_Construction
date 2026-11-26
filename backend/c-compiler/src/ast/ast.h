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
typedef enum { TYPE_ERROR, TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_VOID } Type;   /* void: functions only */

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
    EX_UNARY,    /* UnaryOp      */
    EX_INDEX,    /* ArrayIndex   : a[i]      (bonus) */
    EX_CALL      /* Call         : f(x, y)   (bonus) */
} ExprKind;

typedef struct Expr Expr;

/* Growable list of expressions (printf arguments, call arguments,
 * array initializers). */
typedef struct {
    Expr **items;
    size_t count, capacity;
} ExprList;
struct Expr {
    ExprKind kind;
    int line;
    Type type;                 /* filled in by the semantic analyzer */
    char *tac_name;            /* Identifier / ArrayIndex / Call: unique name chosen by the semantic analyzer */
    union {
        long integer;
        double real;
        int boolean;
        char *name;
        struct { Operator op; Expr *left, *right; } binary;
        struct { Operator op; Expr *operand; } unary;
        struct { char *name; Expr *index; } index;     /* a[i] */
        struct { char *name; ExprList *args; } call;   /* f(x, y) */
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
    ST_SCANF,    /* Scanf    : scanf("%d", &x, ...);  (C style, bonus) */
    ST_FOR,      /* For      : for (init; cond; update) body   (bonus) */
    ST_DO_WHILE, /* DoWhile  : do body while (cond);           (bonus) */
    ST_SWITCH,   /* Switch   : switch (e) { case 1: ... }      (bonus) */
    ST_CASE,     /* Case     : case 1:  /  default:            (bonus) */
    ST_BREAK,    /* Break                                      (bonus) */
    ST_CONTINUE, /* Continue                                   (bonus) */
    ST_CALL,     /* CallStatement : f(x);                      (bonus) */
    ST_ERROR     /* placeholder left by syntax-error recovery */
} StmtKind;

typedef struct Stmt Stmt;

/* Growable list of statements (program body or block body). */
typedef struct {
    Stmt **items;
    size_t count, capacity;
} StmtList;

/* One parameter of a function: int n */
typedef struct {
    Type type;
    char *name;
    int line;
    char *tac_name;      /* filled in by the semantic analyzer */
} Param;

typedef struct {
    Param *items;
    size_t count, capacity;
} ParamList;

struct Stmt {
    StmtKind kind;
    int line;
    union {
        /* length > 0 for an array (int a[5];), whose initializer is init_list */
        struct { Type type; char *name; Expr *init; char *tac_name;
                 long length; ExprList *init_list; } decl;
        /* x = e, a[i] = e, x += e (op is the operator of a compound assignment,
         * or -1), x++ / x-- (incdec is "++" or "--", value is the literal 1) */
        struct { char *name; Expr *index; Expr *value; char *tac_name;
                 int op; const char *incdec; } assign;
        Expr *print;
        struct { Expr *cond; Stmt *then_branch, *else_branch; } if_stmt;
        struct { Expr *cond; Stmt *body; } while_stmt;          /* while and do-while */
        StmtList *block;
        /* for (init; cond; update) body - each part may be NULL */
        struct { Stmt *init; Expr *cond; Stmt *update; Stmt *body; } for_stmt;
        /* switch (value) { body }: the case labels are ST_CASE statements in body */
        struct { Expr *value; StmtList *body; } switch_stmt;
        struct { long value; int is_default; int label; } case_label;  /* label: set by codegen */
        Expr *call;                                   /* ST_CALL: an EX_CALL expression */
        struct { Type type; char *name; ParamList *params; StmtList *body; } function;
        Expr *ret;                                    /* NULL for `return;` */
        /* printf and scanf; format: text between the quotes, escapes kept.
         * For scanf every argument is an Identifier (the variable after '&'). */
        struct { char *format; ExprList *args; } printf_stmt;
    } as;
};

/* Constructors used by the parser actions. */
Expr *ast_int(long value, int line);
Expr *ast_float(double value, int line);
Expr *ast_bool(int value, int line);
Expr *ast_name(char *name, int line);
Expr *ast_binary(Operator op, Expr *left, Expr *right, int line);
Expr *ast_unary(Operator op, Expr *operand, int line);
Expr *ast_index(char *name, Expr *index, int line);
Expr *ast_call(char *name, ExprList *args, int line);

Stmt *ast_decl(Type type, char *name, Expr *init, int line);
Stmt *ast_assign(char *name, Expr *value, int line);
Stmt *ast_array_decl(Type type, char *name, long length, ExprList *init_list, int line);
/* target is an Identifier or ArrayIndex; op is -1 for plain '=' */
Stmt *ast_assign_to(Expr *target, int op, Expr *value, int line);
Stmt *ast_incdec(Expr *target, int increment, int line);
Stmt *ast_for(Stmt *init, Expr *cond, Stmt *update, Stmt *body, int line);
Stmt *ast_do_while(Stmt *body, Expr *cond, int line);
Stmt *ast_switch(Expr *value, StmtList *body, int line);
Stmt *ast_case(long value, int is_default, int line);
Stmt *ast_break(int line);
Stmt *ast_continue(int line);
Stmt *ast_call_stmt(Expr *call, int line);
Stmt *ast_print(Expr *value, int line);
Stmt *ast_if(Expr *cond, Stmt *then_branch, Stmt *else_branch, int line);
Stmt *ast_while(Expr *cond, Stmt *body, int line);
Stmt *ast_block(StmtList *body, int line);
Stmt *ast_error(int line);
Stmt *ast_function(Type type, char *name, ParamList *params, StmtList *body, int line);
Stmt *ast_return(Expr *value, int line);
Stmt *ast_printf(char *format, ExprList *args, int line);
Stmt *ast_scanf(char *format, ExprList *args, int line);

StmtList *ast_list_new(void);
StmtList *ast_list_append(StmtList *list, Stmt *stmt);
ExprList *ast_expr_list_new(void);
ExprList *ast_expr_list_append(ExprList *list, Expr *expr);
ParamList *ast_param_list_new(void);
ParamList *ast_param_list_append(ParamList *list, Type type, char *name, int line);

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

/* Prints the tree as a Graphviz digraph (the --dot option, bonus). */
void ast_print_dot(StmtList *program, FILE *out);

#endif
