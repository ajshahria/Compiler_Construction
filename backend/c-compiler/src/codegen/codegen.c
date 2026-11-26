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
 *     print x               output (the manual's print statement)
 *
 * C style printf (bonus) is split into output instructions:
 *     outs "text"           print literal text (C escapes such as \n kept)
 *     out x                 print a value: int and bool as %d, float as %g
 *     outf x 2              print a float with 2 digits after the point (%.2f; %f is 6)
 * and `return` in main() jumps to a label at the end of main.
 *
 * Bonus constructs:
 *     t1 = a[i]             read an array element (i is an element index)
 *     read a[i]             read input into an array element
 *     a[i] = t1             write an array element
 *     func f:               start of the function f ...
 *     formal x              ... whose next parameter is stored in x
 *     return t1  /  return  leave the function (with a value)
 *     endfunc               end of the function (returns without a value)
 *     param t1              pass an argument, then
 *     t2 = call f 2         call f with the last 2 params (or: call f 2)
 *     if t1 goto L1         conditional jump (switch, do-while)
 * A func ... endfunc block is skipped when execution reaches it: its code
 * only runs through a call.
 *
 *     for (init; c; u) S        do S while (c);       switch (e) { case 1: S1 default: S2 }
 *       <init>                  L1:                     t1 = e
 *     L1:                         <S>                   t2 = t1 == 1
 *       <code for c>            L2:   (continue)        if t2 goto L2
 *       ifFalse c goto L3         <code for c>          goto L3
 *       <S>                       if c goto L1        L2:
 *     L2:   (continue)          L3:   (break)           <S1>
 *       <u>                                           L3:
 *       goto L1                                         <S2>
 *     L3:   (break)                                   L4:   (break)
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
static int return_label;     /* label at the end of main(); 0 until a return needs it */
static Stmt *current_function;   /* the function being translated, or NULL */

/* Targets of break and continue in the innermost loop / switch. */
#define MAX_NESTING 256
static int break_labels[MAX_NESTING], continue_labels[MAX_NESTING];
static int nesting;

static void push_targets(int break_label, int continue_label)
{
    if (nesting < MAX_NESTING) {
        break_labels[nesting] = break_label;
        /* a switch has no continue target: continue goes to the loop around it */
        continue_labels[nesting] = continue_label ? continue_label
                                 : nesting > 0 ? continue_labels[nesting - 1] : 0;
    }
    nesting++;
}

static void pop_targets(void)
{
    nesting--;
}

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

static char *gen_call(Expr *e, int want_value);

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
    case EX_INDEX: {
        char *index = gen_expr(e->as.index.index);
        char *result = new_temp();
        emit("%s = %s[%s]\n", result, e->tac_name, index);
        free(index);
        return result;
    }
    case EX_CALL:
        return gen_call(e, 1);
    }
    return copy_string("?");
}

/* A call: every argument is computed first (an argument may itself contain
 * a call), then passed with param. Returns the temporary holding the result,
 * or NULL when want_value is 0. */
static char *gen_call(Expr *e, int want_value)
{
    ExprList *args = e->as.call.args;
    char **addresses = malloc((args->count ? args->count : 1) * sizeof *addresses);
    if (addresses == NULL) {
        perror("out of memory");
        exit(2);
    }
    for (size_t i = 0; i < args->count; i++)
        addresses[i] = gen_expr(args->items[i]);
    for (size_t i = 0; i < args->count; i++) {
        emit("param %s\n", addresses[i]);
        free(addresses[i]);
    }
    free(addresses);
    if (!want_value) {
        emit("call %s %d\n", e->tac_name, (int)args->count);
        return NULL;
    }
    char *result = new_temp();
    emit("%s = call %s %d\n", result, e->tac_name, (int)args->count);
    return result;
}

/* Emits `name = <expr>`. */
static void gen_assignment(const char *name, Expr *value)
{
    char *address = gen_expr(value);
    emit("%s = %s\n", name, address);
    free(address);
}

/* x = e, a[i] = e, and x op= e / x++ (x = x op e). The index of an array
 * element is computed once. */
static void gen_assign_stmt(Stmt *s)
{
    const char *name = s->as.assign.tac_name;
    char *index = s->as.assign.index ? gen_expr(s->as.assign.index) : NULL;
    char *value = gen_expr(s->as.assign.value);
    if (s->as.assign.op >= 0) {
        char *old;
        if (index != NULL) {
            old = new_temp();
            emit("%s = %s[%s]\n", old, name, index);
        } else {
            old = copy_string(name);
        }
        char *result = new_temp();
        emit("%s = %s %s %s\n", result, old, operator_symbol((Operator)s->as.assign.op), value);
        free(old);
        free(value);
        value = result;
    }
    if (index != NULL)
        emit("%s[%s] = %s\n", name, index, value);
    else
        emit("%s = %s\n", name, value);
    free(index);
    free(value);
}

/* printf("a=%d\n", a) -> outs "a="; out a; outs "\n" */
static void gen_printf(Stmt *s)
{
    const char *format = s->as.printf_stmt.format;
    ExprList *args = s->as.printf_stmt.args;
    char *text = malloc(strlen(format) + 1);    /* literal text waiting to be printed */
    size_t text_length = 0, pos = 0, next_arg = 0;
    FormatPiece piece;
    if (text == NULL) {
        perror("out of memory");
        exit(2);
    }
    while (format_next(format, &pos, &piece) > 0) {
        if (!piece.is_conversion) {
            memcpy(text + text_length, piece.text, piece.length);
            text_length += piece.length;
            continue;
        }
        if (text_length > 0) {
            emit("outs \"%.*s\"\n", (int)text_length, text);
            text_length = 0;
        }
        char *address = gen_expr(args->items[next_arg++]);
        if (piece.conversion == 'f')
            emit("outf %s %d\n", address, piece.precision < 0 ? 6 : piece.precision);
        else
            emit("out %s\n", address);       /* %d %i, and %g (a float prints as %g) */
        free(address);
    }
    if (text_length > 0)
        emit("outs \"%.*s\"\n", (int)text_length, text);
    free(text);
}

static void gen_list(StmtList *list);

static void gen_stmt(Stmt *s)
{
    if (s == NULL)
        return;

    switch (s->kind) {
    case ST_DECL:
        /* A declaration only produces code when it has an initializer;
         * an array's initial values are stored one element at a time. */
        if (s->as.decl.init)
            gen_assignment(s->as.decl.tac_name, s->as.decl.init);
        for (size_t i = 0; s->as.decl.init_list && i < s->as.decl.init_list->count; i++) {
            char *address = gen_expr(s->as.decl.init_list->items[i]);
            emit("%s[%d] = %s\n", s->as.decl.tac_name, (int)i, address);
            free(address);
        }
        break;

    case ST_ASSIGN:
        gen_assign_stmt(s);
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
        push_targets(end_label, start_label);
        gen_stmt(s->as.while_stmt.body);
        pop_targets();
        emit("goto L%d\n", start_label);
        emit("L%d:\n", end_label);
        break;
    }

    case ST_DO_WHILE: {
        int start_label = new_label();
        int continue_label = new_label();
        int end_label = new_label();
        emit("L%d:\n", start_label);
        push_targets(end_label, continue_label);
        gen_stmt(s->as.while_stmt.body);
        pop_targets();
        emit("L%d:\n", continue_label);
        char *cond = gen_expr(s->as.while_stmt.cond);
        emit("if %s goto L%d\n", cond, start_label);
        free(cond);
        emit("L%d:\n", end_label);
        break;
    }

    case ST_FOR: {
        gen_stmt(s->as.for_stmt.init);
        int start_label = new_label();
        int continue_label = new_label();
        int end_label = new_label();
        emit("L%d:\n", start_label);
        if (s->as.for_stmt.cond) {            /* no condition: loop until break */
            char *cond = gen_expr(s->as.for_stmt.cond);
            emit("ifFalse %s goto L%d\n", cond, end_label);
            free(cond);
        }
        push_targets(end_label, continue_label);
        gen_stmt(s->as.for_stmt.body);
        pop_targets();
        emit("L%d:\n", continue_label);
        gen_stmt(s->as.for_stmt.update);
        emit("goto L%d\n", start_label);
        emit("L%d:\n", end_label);
        break;
    }

    case ST_SWITCH: {
        StmtList *body = s->as.switch_stmt.body;
        char *value = gen_expr(s->as.switch_stmt.value);
        int default_label = 0;
        /* one test per case, in order, then a jump to default (or out) */
        for (size_t i = 0; body != NULL && i < body->count; i++) {
            Stmt *item = body->items[i];
            if (item->kind != ST_CASE)
                continue;
            item->as.case_label.label = new_label();
            if (item->as.case_label.is_default) {
                default_label = item->as.case_label.label;
                continue;
            }
            char *test = new_temp();
            emit("%s = %s == %ld\n", test, value, item->as.case_label.value);
            emit("if %s goto L%d\n", test, item->as.case_label.label);
            free(test);
        }
        free(value);
        int end_label = new_label();
        emit("goto L%d\n", default_label ? default_label : end_label);
        push_targets(end_label, 0);
        for (size_t i = 0; body != NULL && i < body->count; i++) {
            Stmt *item = body->items[i];
            if (item->kind == ST_CASE)
                emit("L%d:\n", item->as.case_label.label);    /* cases fall through, as in C */
            else
                gen_stmt(item);
        }
        pop_targets();
        emit("L%d:\n", end_label);
        break;
    }

    case ST_CASE:                   /* handled by ST_SWITCH */
        break;

    case ST_BREAK:
        if (nesting > 0)
            emit("goto L%d\n", break_labels[nesting - 1]);
        break;

    case ST_CONTINUE:
        if (nesting > 0 && continue_labels[nesting - 1])
            emit("goto L%d\n", continue_labels[nesting - 1]);
        break;

    case ST_CALL:
        gen_call(s->as.call, 0);
        break;

    case ST_BLOCK:
        gen_list(s->as.block);
        break;

    case ST_FUNCTION:
        current_function = s;
        if (strcmp(s->as.function.name, "main") == 0) {   /* main(): its body is the program */
            gen_list(s->as.function.body);
            if (return_label)
                emit("L%d:\n", return_label);
        } else {
            ParamList *params = s->as.function.params;
            emit("func %s:\n", s->as.function.name);
            for (size_t i = 0; i < params->count; i++)
                emit("formal %s\n", params->items[i].tac_name);
            gen_list(s->as.function.body);
            emit("endfunc\n");
        }
        current_function = NULL;
        break;

    case ST_RETURN:
        if (current_function != NULL && strcmp(current_function->as.function.name, "main") != 0) {
            if (s->as.ret) {
                char *address = gen_expr(s->as.ret);
                emit("return %s\n", address);
                free(address);
            } else {
                emit("return\n");
            }
            break;
        }
        /* return in main(): the value of main() is not used */
        if (!return_label)
            return_label = new_label();
        emit("goto L%d\n", return_label);
        break;

    case ST_PRINTF:
        gen_printf(s);
        break;

    case ST_SCANF:                  /* scanf("%d %f", &a, &b) -> read a; read b */
        for (size_t i = 0; i < s->as.printf_stmt.args->count; i++) {
            Expr *target = s->as.printf_stmt.args->items[i];
            if (target->kind == EX_INDEX) {      /* &a[i]: read a[i] */
                char *index = gen_expr(target->as.index.index);
                emit("read %s[%s]\n", target->tac_name, index);
                free(index);
            } else {
                emit("read %s\n", target->tac_name);
            }
        }
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
    return_label = 0;
    current_function = NULL;
    nesting = 0;
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
