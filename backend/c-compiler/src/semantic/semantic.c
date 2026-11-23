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
 *   - only one function may be defined: int main(), at the top level
 *   - with main(), only variable declarations may stand outside it
 *   - return only inside main(), with an int value
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
    return t == TYPE_INT ? "an int" : t == TYPE_FLOAT ? "a float" : "a bool";
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

static Type check_binary(Expr *e)
{
    Operator op = e->as.binary.op;
    Type left = check_expr(e->as.binary.left);
    Type right = check_expr(e->as.binary.right);

    /* An operand already has an error: it was reported, so stay quiet. */
    if (left == TYPE_ERROR || right == TYPE_ERROR)
        return TYPE_ERROR;

    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        if (!is_numeric(left) || !is_numeric(right)) {
            semantic_error(e->line, "invalid expression: operator '%s' needs numeric operands, got %s and %s",
                           operator_symbol(op), type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return (left == TYPE_FLOAT || right == TYPE_FLOAT) ? TYPE_FLOAT : TYPE_INT;

    case OP_MOD:
        if (left != TYPE_INT || right != TYPE_INT) {
            semantic_error(e->line, "invalid expression: operator '%%' needs int operands, got %s and %s",
                           type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return TYPE_INT;

    case OP_LT: case OP_GT: case OP_LE: case OP_GE:
        if (!is_numeric(left) || !is_numeric(right)) {
            semantic_error(e->line, "invalid expression: operator '%s' needs numeric operands, got %s and %s",
                           operator_symbol(op), type_name(left), type_name(right));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;

    case OP_EQ: case OP_NE:
        if (!(is_numeric(left) && is_numeric(right)) && left != right) {
            semantic_error(e->line, "invalid expression: cannot compare %s with %s using '%s'",
                           type_name(left), type_name(right), operator_symbol(op));
            return TYPE_ERROR;
        }
        return TYPE_BOOL;

    case OP_AND: case OP_OR:
        if (left != TYPE_BOOL || right != TYPE_BOOL) {
            semantic_error(e->line, "invalid expression: logical operator '%s' needs bool operands, got %s and %s",
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
        if (s == NULL) {
            report_unknown_name(e->as.name, e->line);
            e->type = TYPE_ERROR;
        } else {
            e->type = s->type;
            e->tac_name = s->tac_name;
        }
        break;
    }
    case EX_BINARY: e->type = check_binary(e); break;
    case EX_UNARY:  e->type = check_unary(e);  break;
    }
    return e->type;
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
                           piece.conversion, target->as.name);
        else if (!is_int_conv && t != TYPE_FLOAT)
            semantic_error(s->line, "type mismatch: '%%%c' in scanf reads a float, but '%s' is %s (use %%d)",
                           piece.conversion, target->as.name, with_article(t));
    }
    if (used != count)
        semantic_error(s->line, "scanf format has %d conversion(s) but %d variable(s) were given",
                       (int)used, (int)count);
}

/* A program with main(): only declarations (globals) may stand outside it. */
static void check_program_shape(StmtList *program)
{
    int functions = 0;
    Stmt *first_main = NULL;
    for (size_t i = 0; program != NULL && i < program->count; i++)
        if (program->items[i]->kind == ST_FUNCTION) {
            functions++;
            if (first_main == NULL && strcmp(program->items[i]->as.function.name, "main") == 0)
                first_main = program->items[i];
        }
    if (functions == 0)
        return;            /* the manual's form: a plain list of statements */
    for (size_t i = 0; i < program->count; i++) {
        Stmt *s = program->items[i];
        if (s->kind == ST_FUNCTION && first_main != NULL && s != first_main
            && strcmp(s->as.function.name, "main") == 0)
            semantic_error(s->line, "redefinition of 'main' (first defined at line %d)", first_main->line);
        else if (s->kind != ST_FUNCTION && s->kind != ST_DECL && s->kind != ST_ERROR)
            semantic_error(s->line, "statement outside of main(): only variable declarations may appear outside a function");
    }
}

static void check_list(StmtList *list);

static void check_stmt(Stmt *s)
{
    if (s == NULL)
        return;

    switch (s->kind) {
    case ST_DECL: {
        /* Check the initializer first: in `int x = x;` the right-hand x is
         * not yet declared. */
        Type value = s->as.decl.init ? check_expr(s->as.decl.init) : TYPE_ERROR;

        Symbol *previous = symtab_lookup_current(table, s->as.decl.name);
        if (previous != NULL)
            semantic_error(s->line, "redeclaration of '%s' (already declared at line %d in this scope)",
                           s->as.decl.name, previous->line);
        else
            s->as.decl.tac_name = symtab_insert(table, s->as.decl.name, s->as.decl.type, s->line)->tac_name;

        if (s->as.decl.init && value != TYPE_ERROR && !is_assignable(s->as.decl.type, value))
            semantic_error(s->line, "type mismatch: cannot initialize %s variable '%s' with %s value",
                           type_name(s->as.decl.type), s->as.decl.name, with_article(value));
        break;
    }
    case ST_ASSIGN: {
        Symbol *target = symtab_lookup(table, s->as.assign.name);
        Type value = check_expr(s->as.assign.value);
        if (target == NULL) {
            report_unknown_name(s->as.assign.name, s->line);
            break;
        }
        s->as.assign.tac_name = target->tac_name;
        if (value != TYPE_ERROR && !is_assignable(target->type, value))
            semantic_error(s->line, "invalid assignment: cannot assign %s value to %s variable '%s'",
                           with_article(value), type_name(target->type), target->name);
        break;
    }
    case ST_PRINT:
        check_expr(s->as.print);
        break;
    case ST_IF:
        check_condition(s->as.if_stmt.cond, "if", s->line);
        check_stmt(s->as.if_stmt.then_branch);
        check_stmt(s->as.if_stmt.else_branch);
        break;
    case ST_WHILE:
        check_condition(s->as.while_stmt.cond, "while", s->line);
        check_stmt(s->as.while_stmt.body);
        break;
    case ST_BLOCK:
        symtab_enter_scope(table);
        block_depth++;
        check_list(s->as.block);
        block_depth--;
        symtab_exit_scope(table);
        break;
    case ST_FUNCTION:
        if (block_depth > 0)
            semantic_error(s->line, "function '%s' cannot be defined inside a block", s->as.function.name);
        else if (strcmp(s->as.function.name, "main") != 0)
            semantic_error(s->line, "function '%s': only the function 'main' can be defined (functions are not supported)",
                           s->as.function.name);
        else if (s->as.function.type != TYPE_INT)
            semantic_error(s->line, "'main' must return int, not %s", type_name(s->as.function.type));
        symtab_enter_scope(table);
        block_depth++;
        in_main++;
        check_list(s->as.function.body);
        in_main--;
        block_depth--;
        symtab_exit_scope(table);
        break;
    case ST_RETURN: {
        Type t = s->as.ret ? check_expr(s->as.ret) : TYPE_INT;
        if (!in_main)
            semantic_error(s->line, "'return' outside of main()");
        else if (t != TYPE_ERROR && t != TYPE_INT)
            semantic_error(s->line, "type mismatch: main() must return an int, got %s value", with_article(t));
        break;
    }
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
    symtab_init(table);
    check_program_shape(program);
    check_list(program);
    return error_count;
}
