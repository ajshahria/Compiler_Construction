/*
 * ast.c - construction and printing of AST nodes.
 */
#include "ast.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

Expr *ast_index(char *name, Expr *index, int line)
{
    Expr *e = new_expr(EX_INDEX, line);
    e->as.index.name = name;
    e->as.index.index = index;
    return e;
}

Expr *ast_call(char *name, ExprList *args, int line)
{
    Expr *e = new_expr(EX_CALL, line);
    e->as.call.name = name;
    e->as.call.args = args ? args : ast_expr_list_new();
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
    s->as.assign.op = -1;
    return s;
}

Stmt *ast_array_decl(Type type, char *name, long length, ExprList *init_list, int line)
{
    Stmt *s = ast_decl(type, name, NULL, line);
    /* int a[] = {1, 2, 3}: the length is the number of values */
    s->as.decl.length = length > 0 ? length : (init_list ? (long)init_list->count : 0);
    s->as.decl.init_list = init_list;
    if (s->as.decl.length <= 0)
        s->as.decl.length = -1;            /* reported by the semantic analyzer */
    return s;
}

Stmt *ast_assign_to(Expr *target, int op, Expr *value, int line)
{
    char *name = target->kind == EX_INDEX ? target->as.index.name : target->as.name;
    Stmt *s = ast_assign(name, value, line);
    s->as.assign.index = target->kind == EX_INDEX ? target->as.index.index : NULL;
    s->as.assign.op = op;
    return s;
}

/* x++ and ++x are the statement x += 1 (likewise x-- and --x). */
Stmt *ast_incdec(Expr *target, int increment, int line)
{
    Stmt *s = ast_assign_to(target, increment ? OP_ADD : OP_SUB, ast_int(1, line), line);
    s->as.assign.incdec = increment ? "++" : "--";
    return s;
}

Stmt *ast_for(Stmt *init, Expr *cond, Stmt *update, Stmt *body, int line)
{
    Stmt *s = new_stmt(ST_FOR, line);
    s->as.for_stmt.init = init;
    s->as.for_stmt.cond = cond;
    s->as.for_stmt.update = update;
    s->as.for_stmt.body = body;
    return s;
}

Stmt *ast_do_while(Stmt *body, Expr *cond, int line)
{
    Stmt *s = new_stmt(ST_DO_WHILE, line);
    s->as.while_stmt.cond = cond;
    s->as.while_stmt.body = body;
    return s;
}

Stmt *ast_switch(Expr *value, StmtList *body, int line)
{
    Stmt *s = new_stmt(ST_SWITCH, line);
    s->as.switch_stmt.value = value;
    s->as.switch_stmt.body = body;
    return s;
}

Stmt *ast_case(long value, int is_default, int line)
{
    Stmt *s = new_stmt(ST_CASE, line);
    s->as.case_label.value = value;
    s->as.case_label.is_default = is_default;
    return s;
}

Stmt *ast_break(int line)    { return new_stmt(ST_BREAK, line); }
Stmt *ast_continue(int line) { return new_stmt(ST_CONTINUE, line); }

Stmt *ast_call_stmt(Expr *call, int line)
{
    Stmt *s = new_stmt(ST_CALL, line);
    s->as.call = call;
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

Stmt *ast_function(Type type, char *name, ParamList *params, StmtList *body, int line)
{
    Stmt *s = new_stmt(ST_FUNCTION, line);
    s->as.function.type = type;
    s->as.function.name = name;
    s->as.function.params = params ? params : ast_param_list_new();
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

Stmt *ast_scanf(char *format, ExprList *args, int line)
{
    Stmt *s = new_stmt(ST_SCANF, line);
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

ParamList *ast_param_list_new(void)
{
    return allocate(sizeof(ParamList));
}

ParamList *ast_param_list_append(ParamList *list, Type type, char *name, int line)
{
    if (list == NULL)
        list = ast_param_list_new();
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 4;
        list->items = realloc(list->items, list->capacity * sizeof *list->items);
        if (list->items == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    Param *p = &list->items[list->count++];
    p->type = type;
    p->name = name;
    p->line = line;
    p->tac_name = NULL;
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
    case TYPE_VOID:  return "void";
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

/* ---------- printing (--ast and --dot) ---------- */

/* The listing goes to a FILE (--ast) or into a text buffer that --dot turns
 * into a Graphviz graph, so both show exactly the same nodes. */
static FILE *ast_out;
static char *ast_buffer;
static size_t ast_length, ast_capacity;

static void put(const char *format, ...)
{
    va_list args;
    if (ast_out != NULL) {
        va_start(args, format);
        vfprintf(ast_out, format, args);
        va_end(args);
        return;
    }
    va_start(args, format);
    int need = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (ast_length + (size_t)need + 1 > ast_capacity) {
        ast_capacity = (ast_length + (size_t)need + 1) * 2;
        ast_buffer = realloc(ast_buffer, ast_capacity);
        if (ast_buffer == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    va_start(args, format);
    vsnprintf(ast_buffer + ast_length, (size_t)need + 1, format, args);
    va_end(args);
    ast_length += (size_t)need;
}

static void indent(int depth)
{
    for (int i = 0; i < depth; i++)
        put("  ");
}

static void print_stmt(Stmt *s, int depth);
static void print_list(StmtList *list, int depth);

static void print_expr(Expr *e, int depth)
{
    indent(depth);
    if (e == NULL) {
        put("<missing>\n");
        return;
    }
    switch (e->kind) {
    case EX_INT:
        put("IntLiteral %ld\n", e->as.integer);
        break;
    case EX_FLOAT:
        put("FloatLiteral %g%s\n", e->as.real, e->as.real == (long)e->as.real ? ".0" : "");
        break;
    case EX_BOOL:
        put("BoolLiteral %s\n", e->as.boolean ? "true" : "false");
        break;
    case EX_NAME:
        put("Identifier %s\n", e->as.name);
        break;
    case EX_BINARY:
        put("BinaryOp %s\n", operator_symbol(e->as.binary.op));
        print_expr(e->as.binary.left, depth + 1);
        print_expr(e->as.binary.right, depth + 1);
        break;
    case EX_UNARY:
        put("UnaryOp %s\n", operator_symbol(e->as.unary.op));
        print_expr(e->as.unary.operand, depth + 1);
        break;
    case EX_INDEX:
        put("ArrayIndex %s\n", e->as.index.name);
        print_expr(e->as.index.index, depth + 1);
        break;
    case EX_CALL:
        put("Call %s\n", e->as.call.name);
        for (size_t i = 0; i < e->as.call.args->count; i++)
            print_expr(e->as.call.args->items[i], depth + 1);
        break;
    }
}

/* A labelled child such as "Condition" or "Body". */
static void print_part(const char *title, int depth)
{
    indent(depth);
    put("%s\n", title);
}

static void print_stmt(Stmt *s, int depth)
{
    if (s == NULL)
        return;
    indent(depth);
    switch (s->kind) {
    case ST_DECL:
        if (s->as.decl.length != 0) {
            put("ArrayDecl %s %s[%ld] (line %d)\n", type_name(s->as.decl.type), s->as.decl.name,
                s->as.decl.length, s->line);
            for (size_t i = 0; s->as.decl.init_list && i < s->as.decl.init_list->count; i++)
                print_expr(s->as.decl.init_list->items[i], depth + 1);
            break;
        }
        put("VarDecl %s %s (line %d)\n", type_name(s->as.decl.type), s->as.decl.name, s->line);
        if (s->as.decl.init)
            print_expr(s->as.decl.init, depth + 1);
        break;
    case ST_ASSIGN:
        if (s->as.assign.incdec) {
            put("%s %s%s (line %d)\n", s->as.assign.incdec[0] == '+' ? "Increment" : "Decrement",
                s->as.assign.name, s->as.assign.index ? "[]" : "", s->line);
            if (s->as.assign.index)
                print_expr(s->as.assign.index, depth + 1);
            break;
        }
        if (s->as.assign.op >= 0)
            put("CompoundAssign %s%s %s= (line %d)\n", s->as.assign.name, s->as.assign.index ? "[]" : "",
                operator_symbol((Operator)s->as.assign.op), s->line);
        else
            put("Assign %s%s (line %d)\n", s->as.assign.name, s->as.assign.index ? "[]" : "", s->line);
        if (s->as.assign.index) {
            print_part("Index", depth + 1);
            print_expr(s->as.assign.index, depth + 2);
            print_part("Value", depth + 1);
            print_expr(s->as.assign.value, depth + 2);
        } else {
            print_expr(s->as.assign.value, depth + 1);
        }
        break;
    case ST_PRINT:
        put("Print (line %d)\n", s->line);
        print_expr(s->as.print, depth + 1);
        break;
    case ST_IF:
        put("%s (line %d)\n", s->as.if_stmt.else_branch ? "IfElse" : "If", s->line);
        print_part("Condition", depth + 1);
        print_expr(s->as.if_stmt.cond, depth + 2);
        print_part("Then", depth + 1);
        print_stmt(s->as.if_stmt.then_branch, depth + 2);
        if (s->as.if_stmt.else_branch) {
            print_part("Else", depth + 1);
            print_stmt(s->as.if_stmt.else_branch, depth + 2);
        }
        break;
    case ST_WHILE:
    case ST_DO_WHILE:
        put("%s (line %d)\n", s->kind == ST_WHILE ? "While" : "DoWhile", s->line);
        if (s->kind == ST_DO_WHILE) {
            print_part("Body", depth + 1);
            print_stmt(s->as.while_stmt.body, depth + 2);
        }
        print_part("Condition", depth + 1);
        print_expr(s->as.while_stmt.cond, depth + 2);
        if (s->kind == ST_WHILE) {
            print_part("Body", depth + 1);
            print_stmt(s->as.while_stmt.body, depth + 2);
        }
        break;
    case ST_FOR:
        put("For (line %d)\n", s->line);
        if (s->as.for_stmt.init) {
            print_part("Init", depth + 1);
            print_stmt(s->as.for_stmt.init, depth + 2);
        }
        if (s->as.for_stmt.cond) {
            print_part("Condition", depth + 1);
            print_expr(s->as.for_stmt.cond, depth + 2);
        }
        if (s->as.for_stmt.update) {
            print_part("Update", depth + 1);
            print_stmt(s->as.for_stmt.update, depth + 2);
        }
        print_part("Body", depth + 1);
        print_stmt(s->as.for_stmt.body, depth + 2);
        break;
    case ST_SWITCH:
        put("Switch (line %d)\n", s->line);
        print_part("Value", depth + 1);
        print_expr(s->as.switch_stmt.value, depth + 2);
        print_part("Body", depth + 1);
        print_list(s->as.switch_stmt.body, depth + 2);
        break;
    case ST_CASE:
        if (s->as.case_label.is_default)
            put("Default (line %d)\n", s->line);
        else
            put("Case %ld (line %d)\n", s->as.case_label.value, s->line);
        break;
    case ST_BREAK:
        put("Break (line %d)\n", s->line);
        break;
    case ST_CONTINUE:
        put("Continue (line %d)\n", s->line);
        break;
    case ST_CALL:
        put("CallStatement (line %d)\n", s->line);
        print_expr(s->as.call, depth + 1);
        break;
    case ST_BLOCK:
        put("Block (line %d)\n", s->line);
        print_list(s->as.block, depth + 1);
        break;
    case ST_FUNCTION: {
        ParamList *params = s->as.function.params;
        put("Function %s %s(", type_name(s->as.function.type), s->as.function.name);
        for (size_t i = 0; i < params->count; i++)
            put("%s%s %s", i ? ", " : "", type_name(params->items[i].type), params->items[i].name);
        put(") (line %d)\n", s->line);
        print_list(s->as.function.body, depth + 1);
        break;
    }
    case ST_RETURN:
        put("Return (line %d)\n", s->line);
        if (s->as.ret)
            print_expr(s->as.ret, depth + 1);
        break;
    case ST_PRINTF:
        put("Printf \"%s\" (line %d)\n", s->as.printf_stmt.format, s->line);
        for (size_t i = 0; s->as.printf_stmt.args && i < s->as.printf_stmt.args->count; i++)
            print_expr(s->as.printf_stmt.args->items[i], depth + 1);
        break;
    case ST_SCANF:
        put("Scanf \"%s\" (line %d)\n", s->as.printf_stmt.format, s->line);
        for (size_t i = 0; s->as.printf_stmt.args && i < s->as.printf_stmt.args->count; i++)
            print_expr(s->as.printf_stmt.args->items[i], depth + 1);
        break;
    case ST_ERROR:
        put("ErrorStatement (line %d)\n", s->line);
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
    put("Program\n");
    print_list(program, 1);
}

/* Fill colour of a node in the --dot graph, by the kind of construct. */
static const char *dot_colour(const char *label)
{
    static const char *const KINDS[][2] = {
        { "Program", "#d9d2e9" }, { "Function", "#d9d2e9" },
        { "VarDecl", "#cfe2f3" }, { "ArrayDecl", "#cfe2f3" },
        { "Assign", "#d9ead3" }, { "CompoundAssign", "#d9ead3" },
        { "Increment", "#d9ead3" }, { "Decrement", "#d9ead3" },
        { "If", "#fce5cd" }, { "IfElse", "#fce5cd" }, { "While", "#fce5cd" },
        { "DoWhile", "#fce5cd" }, { "For", "#fce5cd" }, { "Switch", "#fce5cd" },
        { "Case", "#fce5cd" }, { "Default", "#fce5cd" }, { "Break", "#fce5cd" },
        { "Continue", "#fce5cd" }, { "Return", "#fce5cd" },
        { "BinaryOp", "#fff2cc" }, { "UnaryOp", "#fff2cc" },
        { "IntLiteral", "#ffffff" }, { "FloatLiteral", "#ffffff" }, { "BoolLiteral", "#ffffff" },
        { "Identifier", "#ffffff" }, { "ArrayIndex", "#ffffff" }, { "Call", "#ead1dc" },
        { "CallStatement", "#ead1dc" },
    };
    size_t word = strcspn(label, " (");
    for (size_t k = 0; k < sizeof KINDS / sizeof KINDS[0]; k++)
        if (strlen(KINDS[k][0]) == word && strncmp(label, KINDS[k][0], word) == 0)
            return KINDS[k][1];
    return "#f3f3f3";                       /* Condition, Body, Then, ... */
}

/* --dot: the --ast listing as a Graphviz digraph. Every line of the listing
 * is a node; its parent is the nearest line above it indented one level
 * less. Render with:  dot -Tpng ast.dot -o ast.png */
void ast_print_dot(StmtList *program, FILE *out)
{
    ast_out = NULL;
    ast_buffer = NULL;
    ast_length = ast_capacity = 0;
    put("Program\n");
    print_list(program, 1);

    fprintf(out, "digraph AST {\n");
    fprintf(out, "    node [shape=box, style=\"rounded,filled\", fontname=\"Helvetica\", fontsize=11];\n");
    fprintf(out, "    edge [arrowsize=0.7];\n");
    int parents[256];               /* node number at each depth */
    int node = 0;
    for (char *line = ast_buffer; line != NULL && *line != '\0'; node++) {
        char *end = strchr(line, '\n');
        if (end != NULL)
            *end = '\0';
        int depth = 0;
        while (line[depth * 2] == ' ' && line[depth * 2 + 1] == ' ')
            depth++;
        const char *label = line + depth * 2;
        fprintf(out, "    n%d [label=\"", node);
        for (const char *c = label; *c != '\0'; c++) {
            if (*c == '"' || *c == '\\')
                fputc('\\', out);
            fputc(*c, out);
        }
        fprintf(out, "\", fillcolor=\"%s\"];\n", dot_colour(label));
        if (depth > 255)
            depth = 255;
        parents[depth] = node;
        if (depth > 0)
            fprintf(out, "    n%d -> n%d;\n", parents[depth - 1], node);
        line = end ? end + 1 : NULL;
    }
    fprintf(out, "}\n");
    free(ast_buffer);
    ast_buffer = NULL;
}
