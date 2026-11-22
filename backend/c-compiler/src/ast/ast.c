/*
 * ast.c - construction and printing of AST nodes.
 */
#include "ast.h"

#include <stdio.h>
#include <stdlib.h>

/* calloc that stops the compiler cleanly if memory runs out. */
static void *allocate(size_t size)
{
    void *p = calloc(1, size);
    if (p == NULL) {
        perror("out of memory");
        exit(2);
    }
    return p;
}

/* ---------- expression constructors ---------- */

static Expr *new_expr(ExprKind kind, int line)
{
    Expr *e = allocate(sizeof *e);
    e->kind = kind;
    e->line = line;
    e->type = TYPE_ERROR;
    return e;
}

Expr *ast_int(long value, int line)
{
    Expr *e = new_expr(EX_INT, line);
    e->as.integer = value;
    return e;
}

Expr *ast_float(double value, int line)
{
    Expr *e = new_expr(EX_FLOAT, line);
    e->as.real = value;
    return e;
}

Expr *ast_bool(int value, int line)
{
    Expr *e = new_expr(EX_BOOL, line);
    e->as.boolean = value;
    return e;
}

Expr *ast_name(char *name, int line)
{
    Expr *e = new_expr(EX_NAME, line);
    e->as.name = name;
    return e;
}

Expr *ast_binary(Operator op, Expr *left, Expr *right, int line)
{
    Expr *e = new_expr(EX_BINARY, line);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    return e;
}

Expr *ast_unary(Operator op, Expr *operand, int line)
{
    Expr *e = new_expr(EX_UNARY, line);
    e->as.unary.op = op;
    e->as.unary.operand = operand;
    return e;
}

/* ---------- statement constructors ---------- */

static Stmt *new_stmt(StmtKind kind, int line)
{
    Stmt *s = allocate(sizeof *s);
    s->kind = kind;
    s->line = line;
    return s;
}

Stmt *ast_decl(Type type, char *name, Expr *init, int line)
{
    Stmt *s = new_stmt(ST_DECL, line);
    s->as.decl.type = type;
    s->as.decl.name = name;
    s->as.decl.init = init;
    return s;
}

Stmt *ast_assign(char *name, Expr *value, int line)
{
    Stmt *s = new_stmt(ST_ASSIGN, line);
    s->as.assign.name = name;
    s->as.assign.value = value;
    return s;
}

Stmt *ast_print(Expr *value, int line)
{
    Stmt *s = new_stmt(ST_PRINT, line);
    s->as.print = value;
    return s;
}

Stmt *ast_if(Expr *cond, Stmt *then_branch, Stmt *else_branch, int line)
{
    Stmt *s = new_stmt(ST_IF, line);
    s->as.if_stmt.cond = cond;
    s->as.if_stmt.then_branch = then_branch;
    s->as.if_stmt.else_branch = else_branch;
    return s;
}

Stmt *ast_while(Expr *cond, Stmt *body, int line)
{
    Stmt *s = new_stmt(ST_WHILE, line);
    s->as.while_stmt.cond = cond;
    s->as.while_stmt.body = body;
    return s;
}

Stmt *ast_block(StmtList *body, int line)
{
    Stmt *s = new_stmt(ST_BLOCK, line);
    s->as.block = body;
    return s;
}

Stmt *ast_error(int line)
{
    return new_stmt(ST_ERROR, line);
}

Stmt *ast_function(Type type, char *name, StmtList *body, int line)
{
    Stmt *s = new_stmt(ST_FUNCTION, line);
    s->as.function.type = type;
    s->as.function.name = name;
    s->as.function.body = body;
    return s;
}

Stmt *ast_return(Expr *value, int line)
{
    Stmt *s = new_stmt(ST_RETURN, line);
    s->as.ret = value;
    return s;
}

Stmt *ast_printf(char *format, ExprList *args, int line)
{
    Stmt *s = new_stmt(ST_PRINTF, line);
    s->as.printf_stmt.format = format;
    s->as.printf_stmt.args = args;
    return s;
}

ExprList *ast_expr_list_new(void)
{
    return allocate(sizeof(ExprList));
}

ExprList *ast_expr_list_append(ExprList *list, Expr *expr)
{
    if (list == NULL)
        list = ast_expr_list_new();
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 4;
        list->items = realloc(list->items, list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    list->items[list->count++] = expr;
    return list;
}

/* Supported conversions: %d %i (int, bool), %f %g (float), with an optional
 * precision such as %.2f, and %% for a literal percent sign. */
int format_next(const char *format, size_t *pos, FormatPiece *piece)
{
    size_t i = *pos;
    piece->is_conversion = 0;
    piece->conversion = 0;
    piece->precision = -1;
    piece->text = format + i;
    piece->length = 0;
    if (format[i] == '\0')
        return 0;

    if (format[i] != '%') {                 /* literal text up to the next % */
        while (format[i] != '\0' && format[i] != '%')
            i += (format[i] == '\\' && format[i + 1] != '\0') ? 2 : 1;
        piece->length = (size_t)(format + i - piece->text);
        *pos = i;
        return 1;
    }
    i++;                                     /* skip the % */
    if (format[i] == '%') {                  /* %% prints one % */
        piece->text = format + i;
        piece->length = 1;
        *pos = i + 1;
        return 1;
    }
    if (format[i] == '.') {
        int digits = 0, value = 0;
        for (i++; format[i] >= '0' && format[i] <= '9'; i++, digits++)
            value = value * 10 + (format[i] - '0');
        piece->precision = digits ? value : 0;
    }
    piece->conversion = format[i];
    if (format[i] == '\0')
        return -1;
    *pos = i + 1;
    if (format[i] == 'd' || format[i] == 'i') {
        piece->is_conversion = 1;
        return piece->precision < 0 ? 1 : -1;      /* %.2d is not supported */
    }
    if (format[i] == 'f' || format[i] == 'g') {
        piece->is_conversion = 1;
        return 1;
    }
    return -1;
}

StmtList *ast_list_new(void)
{
    return allocate(sizeof(StmtList));
}

StmtList *ast_list_append(StmtList *list, Stmt *stmt)
{
    if (list == NULL)
        list = ast_list_new();
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 8;
        list->items = realloc(list->items, list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    list->items[list->count++] = stmt;
    return list;
}

/* ---------- helpers ---------- */

const char *type_name(Type type)
{
    switch (type) {
    case TYPE_INT:   return "int";
    case TYPE_FLOAT: return "float";
    case TYPE_BOOL:  return "bool";
    default:         return "error";
    }
}

const char *operator_symbol(Operator op)
{
    switch (op) {
    case OP_ADD: return "+";
    case OP_SUB: return "-";
    case OP_MUL: return "*";
    case OP_DIV: return "/";
    case OP_MOD: return "%";
    case OP_LT:  return "<";
    case OP_GT:  return ">";
    case OP_LE:  return "<=";
    case OP_GE:  return ">=";
    case OP_EQ:  return "==";
    case OP_NE:  return "!=";
    case OP_AND: return "&&";
    case OP_OR:  return "||";
    case OP_NOT: return "!";
    case OP_NEG: return "-";
    }
    return "?";
}

/* ---------- printing (--ast) ---------- */

static FILE *ast_out;   /* destination of the --ast listing */
static void indent(int depth)
{
    for (int i = 0; i < depth; i++)
        fprintf(ast_out, "  ");
}

static void print_expr(Expr *e, int depth)
{
    indent(depth);
    if (e == NULL) {
        fprintf(ast_out, "<missing>\n");
        return;
    }
    switch (e->kind) {
    case EX_INT:
        fprintf(ast_out, "IntLiteral %ld\n", e->as.integer);
        break;
    case EX_FLOAT:
        fprintf(ast_out, "FloatLiteral %g%s\n", e->as.real,
                e->as.real == (long)e->as.real ? ".0" : "");
        break;
    case EX_BOOL:
        fprintf(ast_out, "BoolLiteral %s\n", e->as.boolean ? "true" : "false");
        break;
    case EX_NAME:
        fprintf(ast_out, "Identifier %s\n", e->as.name);
        break;
    case EX_BINARY:
        fprintf(ast_out, "BinaryOp %s\n", operator_symbol(e->as.binary.op));
        print_expr(e->as.binary.left, depth + 1);
        print_expr(e->as.binary.right, depth + 1);
        break;
    case EX_UNARY:
        fprintf(ast_out, "UnaryOp %s\n", operator_symbol(e->as.unary.op));
        print_expr(e->as.unary.operand, depth + 1);
        break;
    }
}

static void print_list(StmtList *list, int depth);

static void print_stmt(Stmt *s, int depth)
{
    if (s == NULL)
        return;
    indent(depth);
    switch (s->kind) {
    case ST_DECL:
        fprintf(ast_out, "VarDecl %s %s (line %d)\n", type_name(s->as.decl.type), s->as.decl.name, s->line);
        if (s->as.decl.init)
            print_expr(s->as.decl.init, depth + 1);
        break;
    case ST_ASSIGN:
        fprintf(ast_out, "Assign %s (line %d)\n", s->as.assign.name, s->line);
        print_expr(s->as.assign.value, depth + 1);
        break;
    case ST_PRINT:
        fprintf(ast_out, "Print (line %d)\n", s->line);
        print_expr(s->as.print, depth + 1);
        break;
    case ST_IF:
        fprintf(ast_out, "%s (line %d)\n", s->as.if_stmt.else_branch ? "IfElse" : "If", s->line);
        indent(depth + 1);
        fprintf(ast_out, "Condition\n");
        print_expr(s->as.if_stmt.cond, depth + 2);
        indent(depth + 1);
        fprintf(ast_out, "Then\n");
        print_stmt(s->as.if_stmt.then_branch, depth + 2);
        if (s->as.if_stmt.else_branch) {
            indent(depth + 1);
            fprintf(ast_out, "Else\n");
            print_stmt(s->as.if_stmt.else_branch, depth + 2);
        }
        break;
    case ST_WHILE:
        fprintf(ast_out, "While (line %d)\n", s->line);
        indent(depth + 1);
        fprintf(ast_out, "Condition\n");
        print_expr(s->as.while_stmt.cond, depth + 2);
        indent(depth + 1);
        fprintf(ast_out, "Body\n");
        print_stmt(s->as.while_stmt.body, depth + 2);
        break;
    case ST_BLOCK:
        fprintf(ast_out, "Block (line %d)\n", s->line);
        print_list(s->as.block, depth + 1);
        break;
    case ST_FUNCTION:
        fprintf(ast_out, "Function %s %s() (line %d)\n", type_name(s->as.function.type),
                s->as.function.name, s->line);
        print_list(s->as.function.body, depth + 1);
        break;
    case ST_RETURN:
        fprintf(ast_out, "Return (line %d)\n", s->line);
        if (s->as.ret)
            print_expr(s->as.ret, depth + 1);
        break;
    case ST_PRINTF:
        fprintf(ast_out, "Printf \"%s\" (line %d)\n", s->as.printf_stmt.format, s->line);
        for (size_t i = 0; s->as.printf_stmt.args && i < s->as.printf_stmt.args->count; i++)
            print_expr(s->as.printf_stmt.args->items[i], depth + 1);
        break;
    case ST_ERROR:
        fprintf(ast_out, "ErrorStatement (line %d)\n", s->line);
        break;
    }
}

static void print_list(StmtList *list, int depth)
{
    for (size_t i = 0; list != NULL && i < list->count; i++)
        print_stmt(list->items[i], depth);
}

void ast_print_program(StmtList *program, FILE *out)
{
    ast_out = out;
    fprintf(ast_out, "Program\n");
    print_list(program, 1);
}
