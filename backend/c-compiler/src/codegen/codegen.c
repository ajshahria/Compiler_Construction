/*
 * codegen.c - Three Address Code generator.
 *
 * Every instruction has at most one operator and three addresses:
 *
 *     x = y                 copy
 *     t1 = a + b            binary operation (any arithmetic/relational/logical op)
 *     t2 = - a  /  t2 = ! a unary operation
 *     ifFalse t3 goto L1    conditional jump
 *     goto L2               unconditional jump
 *     L1:                   label
 *     print x               output
 *
 * Variables use the unique TAC name picked by the symbol table, so a
 * variable that shadows an outer one (x and x@3) never shares its storage.
 *
 * Expressions are translated bottom-up: each operator writes its result into a
 * new temporary t1, t2, ... and the temporary's name is returned to the
 * caller. The parser already built the tree with the right precedence, so
 * a post-order walk evaluates operators in the right order.
 *
 * Control flow:
 *     if (c) S              if (c) S1 else S2         while (c) S
 *       <code for c>          <code for c>            L1:
 *       ifFalse c goto L1     ifFalse c goto L1         <code for c>
 *       <S>                   <S1>                      ifFalse c goto L2
 *     L1:                     goto L2                   <S>
 *                           L1:                         goto L1
 *                             <S2>                    L2:
 *                           L2:
 */
#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* The TAC is collected as text, so it can be printed (--tac) or handed to
 * the back end (--opt, --run, --asm). */
static char *code;
static size_t code_length, code_capacity;
static int temp_count;
static int label_count;

/* Appends one formatted TAC line to the code. */
static void emit(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int need = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (code_length + (size_t)need + 1 > code_capacity) {
        code_capacity = (code_length + (size_t)need + 1) * 2;
        code = realloc(code, code_capacity);
        if (code == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    va_start(args, format);
    vsnprintf(code + code_length, (size_t)need + 1, format, args);
    va_end(args);
    code_length += (size_t)need;
}

static char *copy_string(const char *text)
{
    char *copy = malloc(strlen(text) + 1);
    if (copy == NULL) {
        perror("out of memory");
        exit(2);
    }
    strcpy(copy, text);
    return copy;
}

static char *new_temp(void)
{
    char name[32];
    snprintf(name, sizeof name, "t%d", ++temp_count);
    return copy_string(name);
}

static int new_label(void)
{
    return ++label_count;
}

/* Float literals always keep a decimal point so 3.0 is not printed as 3. */
static char *float_text(double value)
{
    char text[64];
    snprintf(text, sizeof text, "%g", value);
    if (strpbrk(text, ".eE") == NULL)
        strcat(text, ".0");
    return copy_string(text);
}

/* Emits the code for an expression and returns the address (variable,
 * constant or temporary) that holds its value. */
static char *gen_expr(Expr *e)
{
    char text[64];

    switch (e->kind) {
    case EX_INT:
        snprintf(text, sizeof text, "%ld", e->as.integer);
        return copy_string(text);
    case EX_FLOAT:
        return float_text(e->as.real);
    case EX_BOOL:
        return copy_string(e->as.boolean ? "true" : "false");
    case EX_NAME:
        return copy_string(e->tac_name ? e->tac_name : e->as.name);
    case EX_BINARY: {
        char *left = gen_expr(e->as.binary.left);
        char *right = gen_expr(e->as.binary.right);
        char *result = new_temp();
        emit("%s = %s %s %s\n", result, left, operator_symbol(e->as.binary.op), right);
        free(left);
        free(right);
        return result;
    }
    case EX_UNARY: {
        char *operand = gen_expr(e->as.unary.operand);
        char *result = new_temp();
        emit("%s = %s %s\n", result, operator_symbol(e->as.unary.op), operand);
        free(operand);
        return result;
    }
    }
    return copy_string("?");
}

/* Emits `name = <expr>`. */
static void gen_assignment(const char *name, Expr *value)
{
    char *address = gen_expr(value);
    emit("%s = %s\n", name, address);
    free(address);
}

static void gen_list(StmtList *list);

static void gen_stmt(Stmt *s)
{
    if (s == NULL)
        return;

    switch (s->kind) {
    case ST_DECL:
        /* A declaration only produces code when it has an initializer. */
        if (s->as.decl.init)
            gen_assignment(s->as.decl.tac_name, s->as.decl.init);
        break;

    case ST_ASSIGN:
        gen_assignment(s->as.assign.tac_name, s->as.assign.value);
        break;

    case ST_PRINT: {
        char *address = gen_expr(s->as.print);
        emit("print %s\n", address);
        free(address);
        break;
    }

    case ST_IF: {
        int else_label = new_label();
        char *cond = gen_expr(s->as.if_stmt.cond);
        emit("ifFalse %s goto L%d\n", cond, else_label);
        free(cond);
        gen_stmt(s->as.if_stmt.then_branch);
        if (s->as.if_stmt.else_branch) {
            int end_label = new_label();
            emit("goto L%d\n", end_label);
            emit("L%d:\n", else_label);
            gen_stmt(s->as.if_stmt.else_branch);
            emit("L%d:\n", end_label);
        } else {
            emit("L%d:\n", else_label);
        }
        break;
    }

    case ST_WHILE: {
        int start_label = new_label();
        int end_label = new_label();
        emit("L%d:\n", start_label);
        char *cond = gen_expr(s->as.while_stmt.cond);
        emit("ifFalse %s goto L%d\n", cond, end_label);
        free(cond);
        gen_stmt(s->as.while_stmt.body);
        emit("goto L%d\n", start_label);
        emit("L%d:\n", end_label);
        break;
    }

    case ST_BLOCK:
        gen_list(s->as.block);
        break;

    case ST_ERROR:
        break;
    }
}

static void gen_list(StmtList *list)
{
    for (size_t i = 0; list != NULL && i < list->count; i++)
        gen_stmt(list->items[i]);
}

char *generate_tac_text(StmtList *program)
{
    code = NULL;
    code_length = code_capacity = 0;
    temp_count = 0;
    label_count = 0;
    emit("%s", "");           /* an empty program still gets an empty string */
    gen_list(program);
    return code;
}

void generate_tac(StmtList *program, FILE *output)
{
    char *text = generate_tac_text(program);
    fputs(text, output);
    free(text);
}
