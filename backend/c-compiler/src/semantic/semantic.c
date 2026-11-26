/*
 * semantic.c - checks the rules the grammar cannot express.
 *
 * Errors detected (manual section 4.5):
 *   - undeclared variable use
 *   - redeclaration in the same scope
 *   - scope violation (using a variable after its block has closed)
 *   - type mismatch in a declaration's initializer
 *   - invalid assignment (value type does not fit the variable)
 *   - invalid expressions (operator applied to operands of the wrong type)
 *   - non-bool condition in if / while
 *
 * C style (bonus):
 *   - functions are defined at the top level; main() returns int and takes
 *     no parameters; with main(), only declarations and other functions may
 *     stand outside it
 *   - return only inside a function, with a value of the function's type
 *     (none for a void function)
 *   - a call names a function, with one argument of a fitting type for each
 *     parameter; a void function cannot be used as a value; main cannot be called
 *   - an array has a positive size and at most that many initial values; an
 *     array is only used with an index, which must be an int; a variable or
 *     function cannot be indexed
 *   - break only inside a loop or switch, continue only inside a loop
 *   - switch on an int; case labels only directly inside a switch, without
 *     duplicates, and at most one default
 *   - ++ / -- and compound assignments (+= ...) follow the typing of the
 *     operator and of assignment: x += e is valid when x = x + e is
 *   - printf needs #include <stdio.h>; its conversions (%d %i %f %g, with an
 *     optional precision such as %.2f) must match the number and types of the
 *     arguments: %d/%i take an int or bool, %f/%g take a float
 *
 * Typing rules:
 *   + - * /      int,int -> int; any mix of int/float -> float
 *   %            int,int -> int
 *   < > <= >=    numeric operands -> bool
 *   == !=        two numbers or two bools -> bool
 *   && || !      bool operands -> bool
 *   unary -      numeric -> same type
 *   An int value may be stored in a float variable (widening); every other
 *   combination must match exactly.
 */
#include "semantic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SymbolTable *table;
static int error_count;
static int in_main;        /* inside the body of main() */
static int block_depth;    /* 0 at the top level of the program */
static int printf_reported;   /* the missing <stdio.h> was already reported */
static Stmt *current_function;   /* the function being checked, or NULL */
static int loop_depth;           /* loops around the current statement    */
static int breakable_depth;      /* loops and switches around it          */

static void semantic_error(int line, const char *format, ...)
{
    va_list args;
    fprintf(stderr, "line %d: semantic error: ", line);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    error_count++;
}

/* "an int", "a float", "a bool" - for readable messages. */
static const char *with_article(Type t)
{
    return t == TYPE_INT ? "an int" : t == TYPE_FLOAT ? "a float" : t == TYPE_VOID ? "no" : "a bool";
}

static int is_numeric(Type t)
{
    return t == TYPE_INT || t == TYPE_FLOAT;
}

/* Can a value of type `value` be stored in a variable of type `target`? */
static int is_assignable(Type target, Type value)
{
    return target == value || (target == TYPE_FLOAT && value == TYPE_INT);
}

/* Reports a name that is not visible here. If it was declared in a block
 * that is already closed, this is a scope violation; otherwise it was never
 * declared (or is used before its declaration). */
static void report_unknown_name(const char *name, int line)
{
    for (Symbol *s = table->first_declared; s != NULL; s = s->next_declared) {
        if (strcmp(s->name, name) == 0) {
            semantic_error(line,
                "scope violation: '%s' (declared at line %d) is not visible outside its block",
                name, s->line);
            return;
        }
    }
    semantic_error(line, "undeclared variable '%s'", name);
}

static Type check_expr(Expr *e);
static Type check_index(Expr *e);
static Type check_call(Expr *e);
static void check_stmt(Stmt *s);

/* Type of `left op right` (op an arithmetic, relational or logical
 * operator), or TYPE_ERROR after reporting an invalid expression. */
static Type binary_type(Operator op, Type left, Type right, int line);

static Type check_binary(Expr *e)
{
    Type left = check_expr(e->as.binary.left);
    Type right = check_expr(e->as.binary.right);

    /* An operand already has an error: it was reported, so stay quiet. */
    if (left == TYPE_ERROR || right == TYPE_ERROR)
        return TYPE_ERROR;
    return binary_type(e->as.binary.op, left, right, e->line);
}

static Type binary_type(Operator op, Type left, Type right, int line)
{
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        if (!is_numeric(left) || !is_numeric(right)) {
            semantic_error(line, "invalid expression: operator '%s' needs numeric operands, got %s and %s",
                           operator_symbol(op), type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return (left == TYPE_FLOAT || right == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;

    case OP_MOD:
        if (left != TYPE_INT || right != TYPE_INT) {
            semantic_error(line, "invalid expression: operator '%%' needs int operands, got %s and %s",
                           type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return TYPE_INT;

    case OP_LT: case OP_GT: case OP_LE: case OP_GE:
        if (!is_numeric(left) || !is_numeric(right)) {
            semantic_error(line, "invalid expression: operator '%s' needs numeric operands, got %s and %s",
                           operator_symbol(op), type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;

    case OP_EQ: case OP_NE:
        if (!(is_numeric(left) && is_numeric(right)) && left != right) {
            semantic_error(line, "invalid expression: cannot compare %s with %s using '%s'",
                           type_name(left), type_name(right), operator_symbol(op));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;

    case OP_AND: case OP_OR:
        if (left != TYPE_BOOL || right != TYPE_BOOL) {
            semantic_error(line, "invalid expression: logical operator '%s' needs bool operands, got %s and %s",
                           operator_symbol(op), type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;

    default:
        return TYPE_ERROR;
    }
}

static Type check_unary(Expr *e)
{
    Type operand = check_expr(e->as.unary.operand);
    if (operand == TYPE_ERROR)
        return TYPE_ERROR;

    if (e->as.unary.op == OP_NOT) {
        if (operand != TYPE_BOOL) {
            semantic_error(e->line, "invalid expression: operator '!' needs a bool operand, got %s",
                           type_name(operand));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;
    }
    if (!is_numeric(operand)) {           /* unary minus */
        semantic_error(e->line, "invalid expression: unary '-' needs a numeric operand, got %s",
                       type_name(operand));
        return TYPE_ERROR;
    }
    return operand;
}

/* Computes, stores and returns the type of an expression. */
static Type check_expr(Expr *e)
{
    if (e == NULL)
        return TYPE_ERROR;

    switch (e->kind) {
    case EX_INT:   e->type = TYPE_INT;   break;
    case EX_FLOAT: e->type = TYPE_FLOAT; break;
    case EX_BOOL:  e->type = TYPE_BOOL;  break;
    case EX_NAME: {
        Symbol *s = symtab_lookup(table, e->as.name);
        e->type = TYPE_ERROR;
        if (s == NULL)
            report_unknown_name(e->as.name, e->line);
        else if (s->is_function)
            semantic_error(e->line, "invalid expression: function '%s' used without a call (write %s(...))",
                           e->as.name, e->as.name);
        else if (s->length > 0)
            semantic_error(e->line, "invalid expression: array '%s' used without an index (write %s[i])",
                           e->as.name, e->as.name);
        else {
            e->type = s->type;
            e->tac_name = s->tac_name;
        }
        break;
    }
    case EX_BINARY: e->type = check_binary(e); break;
    case EX_UNARY:  e->type = check_unary(e);  break;
    case EX_INDEX:  e->type = check_index(e);  break;
    case EX_CALL:
        e->type = check_call(e);
        if (e->type == TYPE_VOID) {
            semantic_error(e->line, "invalid expression: void function '%s' does not return a value",
                           e->as.call.name);
            e->type = TYPE_ERROR;
        }
        break;
    }
    return e->type;
}

/* The variable an Identifier or ArrayIndex expression names. */
static const char *expr_name(Expr *e)
{
    return e->kind == EX_INDEX ? e->as.index.name : e->as.name;
}

/* An array name with an int index: a[i]. Returns the element type. */
static Type check_index(Expr *e)
{
    Symbol *s = symtab_lookup(table, e->as.index.name);
    Type index = check_expr(e->as.index.index);
    if (s == NULL) {
        report_unknown_name(e->as.index.name, e->line);
        return TYPE_ERROR;
    }
    if (s->is_function || s->length == 0) {
        semantic_error(e->line, "invalid expression: '%s' is not an array and cannot be indexed",
                       e->as.index.name);
        return TYPE_ERROR;
    }
    if (index != TYPE_ERROR && index != TYPE_INT)
        semantic_error(e->line, "type mismatch: index of array '%s' must be an int, got %s",
                       e->as.index.name, type_name(index));
    e->tac_name = s->tac_name;
    return s->type;
}

/* A call f(a, b): one argument of a fitting type per parameter. Returns the
 * function's return type (TYPE_VOID for a void function). */
static Type check_call(Expr *e)
{
    const char *name = e->as.call.name;
    ExprList *args = e->as.call.args;
    Symbol *s = symtab_lookup(table, name);
    for (size_t i = 0; i < args->count; i++)
        check_expr(args->items[i]);
    if (s == NULL) {
        semantic_error(e->line, "undeclared function '%s'", name);
        return TYPE_ERROR;
    }
    if (!s->is_function) {
        semantic_error(e->line, "invalid expression: '%s' is not a function and cannot be called", name);
        return TYPE_ERROR;
    }
    if (strcmp(name, "main") == 0) {
        semantic_error(e->line, "invalid expression: 'main' cannot be called");
        return TYPE_ERROR;
    }
    ParamList *params = s->params;
    if (args->count != params->count) {
        semantic_error(e->line, "function '%s' takes %d argument(s) but %d were given",
                       name, (int)params->count, (int)args->count);
        return s->type;
    }
    for (size_t i = 0; i < args->count; i++) {
        Type t = args->items[i]->type;
        if (t != TYPE_ERROR && !is_assignable(params->items[i].type, t))
            semantic_error(e->line, "type mismatch: argument %d of '%s' must be %s, got %s value",
                           (int)i + 1, name, with_article(params->items[i].type), with_article(t));
    }
    e->tac_name = s->tac_name;
    return s->type;
}

static void check_condition(Expr *cond, const char *keyword, int line)
{
    Type t = check_expr(cond);
    if (t != TYPE_ERROR && t != TYPE_BOOL)
        semantic_error(line, "type mismatch: %s condition must be bool, got %s", keyword, type_name(t));
}

/* printf: the format's conversions against the arguments. */
static void check_printf(Stmt *s)
{
    const char *format = s->as.printf_stmt.format;
    ExprList *args = s->as.printf_stmt.args;
    size_t count = args ? args->count : 0;
    Type *types = NULL;

    if (!header_included("stdio.h") && !printf_reported) {
        semantic_error(s->line, "'printf' is not declared: add #include <stdio.h>");
        printf_reported = 1;             /* once, not for every printf */
    }
    if (count > 0) {
        types = malloc(count * sizeof *types);
        if (types == NULL) {
            perror("out of memory");
            exit(2);
        }
    }
    for (size_t i = 0; i < count; i++)
        types[i] = check_expr(args->items[i]);

    size_t pos = 0, used = 0;
    FormatPiece piece;
    int result;
    while ((result = format_next(format, &pos, &piece)) != 0) {
        if (result < 0) {
            if (piece.conversion == 0)
                semantic_error(s->line, "printf format ends with an incomplete conversion '%%'");
            else
                semantic_error(s->line, "unsupported conversion '%%%s%c' in printf format (use %%d, %%i, %%f, %%g, %%.Nf or %%%%)",
                               piece.precision >= 0 ? "." : "", piece.conversion);
            free(types);
            return;
        }
        if (!piece.is_conversion)
            continue;
        if (used >= count) {
            used++;
            continue;
        }
        Type t = types[used];
        int is_int_conv = piece.conversion == 'd' || piece.conversion == 'i';
        if (t != TYPE_ERROR && is_int_conv && t == TYPE_FLOAT)
            semantic_error(s->line, "type mismatch: '%%%c' in printf expects an int, got a float (use %%f)",
                           piece.conversion);
        else if (t != TYPE_ERROR && !is_int_conv && t != TYPE_FLOAT)
            semantic_error(s->line, "type mismatch: '%%%c' in printf expects a float, got %s (use %%d)",
                           piece.conversion, with_article(t));
        used++;
    }
    if (used != count)
        semantic_error(s->line, "printf format has %d conversion(s) but %d argument(s) were given",
                       (int)used, (int)count);
    free(types);
}

/* scanf: only conversions (%d %i for int/bool, %f %g for float) and spaces in
 * the format, one declared variable (&x) per conversion, of a matching type. */
static void check_scanf(Stmt *s)
{
    const char *format = s->as.printf_stmt.format;
    ExprList *args = s->as.printf_stmt.args;
    size_t count = args ? args->count : 0;

    if (!header_included("stdio.h") && !printf_reported) {
        semantic_error(s->line, "'scanf' is not declared: add #include <stdio.h>");
        printf_reported = 1;
    }
    for (size_t i = 0; i < count; i++)
        check_expr(args->items[i]);      /* each is a variable: reports an undeclared one */

    size_t pos = 0, used = 0;
    FormatPiece piece;
    int result;
    while ((result = format_next(format, &pos, &piece)) != 0) {
        if (result < 0 || (piece.is_conversion && piece.precision >= 0)) {
            if (piece.conversion == 0)
                semantic_error(s->line, "scanf format ends with an incomplete conversion '%%'");
            else if (piece.precision >= 0)
                semantic_error(s->line, "unsupported conversion '%%.%d%c' in scanf format (scanf takes no precision: use %%f)",
                               piece.precision, piece.conversion);
            else
                semantic_error(s->line, "unsupported conversion '%%%c' in scanf format (use %%d, %%i or %%f)",
                               piece.conversion);
            return;
        }
        if (!piece.is_conversion) {
            /* only spaces between conversions: \n and \t count as spaces */
            for (size_t k = 0; k < piece.length; k++) {
                char c = piece.text[k];
                if (c == '\\' && k + 1 < piece.length && strchr("nt", piece.text[k + 1]) != NULL) {
                    k++;
                    continue;
                }
                if (c != ' ') {
                    semantic_error(s->line, "scanf format may contain only conversions (%%d, %%i, %%f) and spaces");
                    return;
                }
            }
            continue;
        }
        if (used >= count) {
            used++;
            continue;
        }
        Expr *target = args->items[used++];
        Type t = target->type;
        int is_int_conv = piece.conversion == 'd' || piece.conversion == 'i';
        if (t == TYPE_ERROR)
            continue;
        if (is_int_conv && t == TYPE_FLOAT)
            semantic_error(s->line, "type mismatch: '%%%c' in scanf reads an int, but '%s' is a float (use %%f)",
                           piece.conversion, expr_name(target));
        else if (!is_int_conv && t != TYPE_FLOAT)
            semantic_error(s->line, "type mismatch: '%%%c' in scanf reads a float, but '%s' is %s (use %%d)",
                           piece.conversion, expr_name(target), with_article(t));
    }
    if (used != count)
        semantic_error(s->line, "scanf format has %d conversion(s) but %d variable(s) were given",
                       (int)used, (int)count);
}

/* A program with main(): only declarations (globals) and other functions may
 * stand outside it. Every function is entered in the global scope before
 * the program is checked, so a function can call one defined further down
 * (and itself: recursion). */
static void check_program_shape(StmtList *program)
{
    int has_main = 0;
    for (size_t i = 0; program != NULL && i < program->count; i++)
        if (program->items[i]->kind == ST_FUNCTION && strcmp(program->items[i]->as.function.name, "main") == 0)
            has_main = 1;
    /* in source order, so the messages come out in line order */
    for (size_t i = 0; program != NULL && i < program->count; i++) {
        Stmt *s = program->items[i];
        if (s->kind != ST_FUNCTION) {
            /* without main(): the manual's form, a plain list of statements (and functions) */
            if (has_main && s->kind != ST_DECL && s->kind != ST_ERROR)
                semantic_error(s->line, "statement outside of main(): only declarations and functions may appear outside a function");
            continue;
        }
        const char *name = s->as.function.name;
        Symbol *previous = symtab_lookup_current(table, name);
        if (previous != NULL && strcmp(name, "main") == 0) {
            semantic_error(s->line, "redefinition of 'main' (first defined at line %d)", previous->line);
            continue;
        }
        if (previous != NULL) {
            semantic_error(s->line, "redefinition of function '%s' (first defined at line %d)",
                           name, previous->line);
            continue;
        }
        Symbol *f = symtab_insert(table, s->as.function.name, s->as.function.type, s->line);
        f->is_function = 1;
        f->params = s->as.function.params;
    }
}

static void check_list(StmtList *list);

/* Adds a declared variable (or array) to the current scope. */
static Symbol *declare(char *name, Type type, long length, int line)
{
    Symbol *previous = symtab_lookup_current(table, name);
    if (previous != NULL) {
        semantic_error(line, "redeclaration of '%s' (already declared at line %d in this scope)",
                       name, previous->line);
        return NULL;
    }
    Symbol *s = symtab_insert(table, name, type, line);
    s->length = length > 0 ? length : 0;
    /* locals of a function other than main are saved around recursive calls */
    if (current_function != NULL && strcmp(current_function->as.function.name, "main") != 0)
        s->function = current_function->as.function.name;
    return s;
}

static void check_array_decl(Stmt *s)
{
    ExprList *init = s->as.decl.init_list;
    for (size_t i = 0; init != NULL && i < init->count; i++) {
        Type t = check_expr(init->items[i]);
        if (t != TYPE_ERROR && !is_assignable(s->as.decl.type, t))
            semantic_error(s->line, "type mismatch: value %d in the initializer of %s array '%s' is %s value",
                           (int)i + 1, type_name(s->as.decl.type), s->as.decl.name, with_article(t));
    }
    if (s->as.decl.length < 0)
        semantic_error(s->line, "array '%s' must have a positive size", s->as.decl.name);
    else if (init != NULL && (long)init->count > s->as.decl.length)
        semantic_error(s->line, "too many values in the initializer of array '%s' (size %ld, %d values)",
                       s->as.decl.name, s->as.decl.length, (int)init->count);
    Symbol *symbol = declare(s->as.decl.name, s->as.decl.type, s->as.decl.length, s->line);
    if (symbol != NULL)
        s->as.decl.tac_name = symbol->tac_name;
}

/* x = e, a[i] = e, x += e, x++ ... */
static void check_assign(Stmt *s)
{
    const char *name = s->as.assign.name;
    Symbol *target = symtab_lookup(table, name);
    Type index = s->as.assign.index ? check_expr(s->as.assign.index) : TYPE_INT;
    Type value = check_expr(s->as.assign.value);
    if (target == NULL) {
        report_unknown_name(name, s->line);
        return;
    }
    if (target->is_function) {
        semantic_error(s->line, "invalid assignment: '%s' is a function, not a variable", name);
        return;
    }
    if (s->as.assign.index != NULL && target->length == 0) {
        semantic_error(s->line, "invalid assignment: '%s' is not an array and cannot be indexed", name);
        return;
    }
    if (s->as.assign.index == NULL && target->length > 0) {
        semantic_error(s->line, "invalid assignment: cannot assign to the whole array '%s' (assign %s[i])",
                       name, name);
        return;
    }
    if (index != TYPE_ERROR && index != TYPE_INT)
        semantic_error(s->line, "type mismatch: index of array '%s' must be an int, got %s",
                       name, type_name(index));
    s->as.assign.tac_name = target->tac_name;
    if (value == TYPE_ERROR)
        return;

    if (s->as.assign.incdec != NULL) {               /* x++ / x-- */
        if (!is_numeric(target->type))
            semantic_error(s->line, "invalid expression: operator '%s' needs a numeric variable, but '%s' is %s",
                           s->as.assign.incdec, name, with_article(target->type));
        return;
    }
    if (s->as.assign.op >= 0) {                      /* x op= e  is  x = x op e */
        value = binary_type((Operator)s->as.assign.op, target->type, value, s->line);
        if (value == TYPE_ERROR)
            return;
    }
    if (!is_assignable(target->type, value))
        semantic_error(s->line, "invalid assignment: cannot assign %s value to %s %s '%s'",
                       with_article(value), type_name(target->type),
                       target->length > 0 ? "array" : "variable", name);
}

/* switch: an int value; the case labels directly in its body. */
static void check_switch(Stmt *s)
{
    Type t = check_expr(s->as.switch_stmt.value);
    if (t != TYPE_ERROR && t != TYPE_INT)
        semantic_error(s->line, "type mismatch: switch value must be an int, got %s", type_name(t));

    StmtList *body = s->as.switch_stmt.body;
    Stmt *first_default = NULL;
    symtab_enter_scope(table);
    block_depth++;
    breakable_depth++;
    for (size_t i = 0; body != NULL && i < body->count; i++) {
        Stmt *item = body->items[i];
        if (item->kind != ST_CASE) {
            check_stmt(item);
            continue;
        }
        if (item->as.case_label.is_default) {
            if (first_default != NULL)
                semantic_error(item->line, "multiple default labels in one switch (first at line %d)",
                               first_default->line);
            else
                first_default = item;
            continue;
        }
        for (size_t j = 0; j < i; j++) {
            Stmt *other = body->items[j];
            if (other->kind == ST_CASE && !other->as.case_label.is_default
                && other->as.case_label.value == item->as.case_label.value) {
                semantic_error(item->line, "duplicate case value %ld (already used at line %d)",
                               item->as.case_label.value, other->line);
                break;
            }
        }
    }
    breakable_depth--;
    block_depth--;
    symtab_exit_scope(table);
}

static void check_function(Stmt *s)
{
    const char *name = s->as.function.name;
    int is_main = strcmp(name, "main") == 0;
    if (block_depth > 0)
        semantic_error(s->line, "function '%s' cannot be defined inside a block", name);
    else if (is_main && s->as.function.type != TYPE_INT)
        semantic_error(s->line, "'main' must return int, not %s", type_name(s->as.function.type));
    else if (is_main && s->as.function.params->count > 0)
        semantic_error(s->line, "'main' must not take parameters");

    Stmt *outer_function = current_function;
    int outer_loops = loop_depth, outer_breakable = breakable_depth;
    current_function = s;
    loop_depth = breakable_depth = 0;
    symtab_enter_scope(table);
    block_depth++;
    in_main += is_main;
    ParamList *params = s->as.function.params;
    for (size_t i = 0; i < params->count; i++) {
        Symbol *p = declare(params->items[i].name, params->items[i].type, 0, params->items[i].line);
        if (p != NULL)
            params->items[i].tac_name = p->tac_name;
    }
    check_list(s->as.function.body);
    in_main -= is_main;
    block_depth--;
    symtab_exit_scope(table);
    current_function = outer_function;
    loop_depth = outer_loops;
    breakable_depth = outer_breakable;
}

static void check_return(Stmt *s)
{
    Type t = s->as.ret ? check_expr(s->as.ret) : TYPE_VOID;
    if (current_function == NULL) {
        semantic_error(s->line, "'return' outside of a function");
        return;
    }
    const char *name = current_function->as.function.name;
    Type expected = current_function->as.function.type;
    if (in_main && strcmp(name, "main") == 0) {
        if (t != TYPE_ERROR && t != TYPE_INT && t != TYPE_VOID)
            semantic_error(s->line, "type mismatch: main() must return an int, got %s value", with_article(t));
        return;
    }
    if (expected == TYPE_VOID && t != TYPE_VOID)
        semantic_error(s->line, "void function '%s' cannot return a value", name);
    else if (expected != TYPE_VOID && t == TYPE_VOID)
        semantic_error(s->line, "function '%s' must return %s value", name, with_article(expected));
    else if (t != TYPE_ERROR && t != TYPE_VOID && !is_assignable(expected, t))
        semantic_error(s->line, "type mismatch: function '%s' returns %s, got %s value",
                       name, type_name(expected), with_article(t));
}

static void check_stmt(Stmt *s)
{
    if (s == NULL)
        return;

    switch (s->kind) {
    case ST_DECL: {
        if (s->as.decl.length != 0) {
            check_array_decl(s);
            break;
        }
        /* Check the initializer first: in `int x = x;` the right-hand x is
         * not yet declared. */
        Type value = s->as.decl.init ? check_expr(s->as.decl.init) : TYPE_ERROR;
        Symbol *symbol = declare(s->as.decl.name, s->as.decl.type, 0, s->line);
        if (symbol != NULL)
            s->as.decl.tac_name = symbol->tac_name;

        if (s->as.decl.init && value != TYPE_ERROR && !is_assignable(s->as.decl.type, value))
            semantic_error(s->line, "type mismatch: cannot initialize %s variable '%s' with %s value",
                           type_name(s->as.decl.type), s->as.decl.name, with_article(value));
        break;
    }
    case ST_ASSIGN:
        check_assign(s);
        break;
    case ST_PRINT:
        check_expr(s->as.print);
        break;
    case ST_IF:
        check_condition(s->as.if_stmt.cond, "if", s->line);
        check_stmt(s->as.if_stmt.then_branch);
        check_stmt(s->as.if_stmt.else_branch);
        break;
    case ST_WHILE:
    case ST_DO_WHILE:
        check_condition(s->as.while_stmt.cond, s->kind == ST_WHILE ? "while" : "do-while", s->line);
        loop_depth++;
        breakable_depth++;
        check_stmt(s->as.while_stmt.body);
        loop_depth--;
        breakable_depth--;
        break;
    case ST_FOR:
        /* for (int i = 0; ...): i belongs to the loop, not to the enclosing block */
        symtab_enter_scope(table);
        block_depth++;
        check_stmt(s->as.for_stmt.init);
        if (s->as.for_stmt.cond)
            check_condition(s->as.for_stmt.cond, "for", s->line);
        check_stmt(s->as.for_stmt.update);
        loop_depth++;
        breakable_depth++;
        check_stmt(s->as.for_stmt.body);
        loop_depth--;
        breakable_depth--;
        block_depth--;
        symtab_exit_scope(table);
        break;
    case ST_SWITCH:
        check_switch(s);
        break;
    case ST_CASE:
        semantic_error(s->line, "'%s' label not directly inside a switch",
                       s->as.case_label.is_default ? "default" : "case");
        break;
    case ST_BREAK:
        if (breakable_depth == 0)
            semantic_error(s->line, "'break' outside of a loop or switch");
        break;
    case ST_CONTINUE:
        if (loop_depth == 0)
            semantic_error(s->line, "'continue' outside of a loop");
        break;
    case ST_CALL:
        check_call(s->as.call);       /* a void function may be called as a statement */
        break;
    case ST_BLOCK:
        symtab_enter_scope(table);
        block_depth++;
        check_list(s->as.block);
        block_depth--;
        symtab_exit_scope(table);
        break;
    case ST_FUNCTION:
        check_function(s);
        break;
    case ST_RETURN:
        check_return(s);
        break;
    case ST_PRINTF:
        check_printf(s);
        break;
    case ST_SCANF:
        check_scanf(s);
        break;
    case ST_ERROR:
        break;
    }
}

static void check_list(StmtList *list)
{
    for (size_t i = 0; list != NULL && i < list->count; i++)
        check_stmt(list->items[i]);
}

int semantic_check(StmtList *program, SymbolTable *symbols)
{
    table = symbols;
    error_count = 0;
    in_main = 0;
    block_depth = 0;
    printf_reported = 0;
    current_function = NULL;
    loop_depth = breakable_depth = 0;
    symtab_init(table);
    check_program_shape(program);
    check_list(program);
    return error_count;
}
