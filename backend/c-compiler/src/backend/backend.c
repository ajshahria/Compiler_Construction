/*
 * backend.c - back end of the compiler: optimizer, interpreter and x86
 * assembly generator for the Three Address Code. See backend.h.
 *
 * Plain C99 that also compiles as C++ (the Java compiler builds this same
 * file as backend.cpp).
 */
#include "backend.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_LEN    128          /* longest variable or label name */
#define MAX_STEPS   10000000L    /* the interpreter stops an endless loop */
#define MAX_OUTPUT  (1 << 20)    /* and a program that prints more than 1 MB */
#define MAX_CALLS   10000        /* and a recursion that goes this deep */

/* ------------------------------------------------------------------ */
/* Growing text buffer                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len, cap;
} Text;

static void *xalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL) {
        perror("out of memory");
        exit(2);
    }
    return p;
}

static void *xrealloc(void *old, size_t size)
{
    void *p = realloc(old, size);
    if (p == NULL) {
        perror("out of memory");
        exit(2);
    }
    return p;
}

static void text_add(Text *t, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int need = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (t->len + (size_t)need + 1 > t->cap) {
        t->cap = (t->len + (size_t)need + 1) * 2;
        t->data = (char *)xrealloc(t->data, t->cap);
    }
    va_start(args, format);
    vsnprintf(t->data + t->len, (size_t)need + 1, format, args);
    va_end(args);
    t->len += (size_t)need;
}

/* The finished text, never NULL (caller frees). */
static char *text_take(Text *t)
{
    if (t->data == NULL) {
        t->data = (char *)xalloc(1);
        t->data[0] = '\0';
    }
    return t->data;
}

/* ------------------------------------------------------------------ */
/* Intermediate representation                                         */
/* ------------------------------------------------------------------ */

typedef enum { OPD_NONE, OPD_NAME, OPD_INT, OPD_FLOAT, OPD_BOOL } OperandKind;

typedef struct {
    OperandKind kind;
    char name[NAME_LEN];   /* OPD_NAME */
    long long iv;          /* OPD_INT value, or 0/1 for OPD_BOOL */
    double fv;             /* OPD_FLOAT value */
    TacType type;
    int slot;              /* OPD_NAME: index into the program's cell table */
} Operand;

typedef enum {
    OP_ASSIGN,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,     /* arithmetic */
    OP_LT, OP_GT, OP_LE, OP_GE, OP_EQ, OP_NE,   /* comparison */
    OP_AND, OP_OR,                              /* logical    */
    OP_NEG, OP_NOT,                             /* unary      */
    OP_LABEL, OP_GOTO, OP_IFFALSE, OP_IFTRUE, OP_PRINT, OP_READ,
    OP_OUTS, OP_OUT, OP_OUTF,                   /* printf pieces (C style) */
    OP_LOAD,      /* result = a[b]     (arrays, bonus)                   */
    OP_STORE,     /* result[a] = b                                       */
    OP_READA,     /* read result[a]                                      */
    OP_FUNC,      /* func label:       (functions, bonus)                */
    OP_FORMAL,    /* formal result     the next argument of the call     */
    OP_RETURN,    /* return a          (a may be absent)                 */
    OP_ENDFUNC,   /* endfunc                                             */
    OP_PARAM,     /* param a                                             */
    OP_CALL       /* result = call label count   (result may be absent)  */
} OpCode;

/* Operator text for OP_ADD .. OP_NOT, indexed by OpCode. */
static const char *const OP_SYMBOL[] = {
    "", "+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||", "-", "!"
};

typedef struct {
    OpCode op;
    Operand result, a, b;
    TacType ty;            /* type the operation works in */
    char label[NAME_LEN];  /* OP_LABEL / OP_GOTO / OP_IF* target; OP_FUNC / OP_CALL function */
    char *str;             /* OP_OUTS: the text, C escapes kept (owned by the program) */
    int precision;         /* OP_OUTF: digits after the decimal point; OP_CALL: argument count */
} Instr;

/* Every variable and temporary: its name and type. A declared variable keeps
 * the type from the symbol table; a temporary takes the type of its value. */
typedef struct {
    char name[NAME_LEN];
    TacType type;
    int declared;
    int length;              /* array: number of elements (0 for a variable) */
    int is_function;         /* the name of a function: type is its return type */
    char owner[NAME_LEN];    /* function whose local this is ("" = global) */
} Cell;

struct TacProgram {
    Instr *code;
    int count, cap;
    Cell *cells;
    int cell_count, cell_cap;
};

static int is_binary(OpCode op)  { return op >= OP_ADD && op <= OP_OR; }
static int is_compare(OpCode op) { return op >= OP_LT && op <= OP_NE; }
/* Does the instruction store a value into the variable or temporary
 * in->result? (a[i] = x and read a[i] store into an element instead.) */
static int defines_result(const Instr *in)
{
    OpCode op = in->op;
    return op == OP_ASSIGN || op == OP_READ || op == OP_NEG || op == OP_NOT || is_binary(op)
        || op == OP_LOAD || op == OP_FORMAL || (op == OP_CALL && in->result.kind == OPD_NAME);
}
static int is_jump(OpCode op) { return op == OP_GOTO || op == OP_IFFALSE || op == OP_IFTRUE; }
static int is_const(const Operand *o)
{
    return o->kind == OPD_INT || o->kind == OPD_FLOAT || o->kind == OPD_BOOL;
}

static int cell_find(const TacProgram *p, const char *name)
{
    for (int k = 0; k < p->cell_count; k++)
        if (strcmp(p->cells[k].name, name) == 0)
            return k;
    return -1;
}

static int cell_add(TacProgram *p, const char *name, TacType type, int declared)
{
    if (p->cell_count == p->cell_cap) {
        p->cell_cap = p->cell_cap ? p->cell_cap * 2 : 32;
        p->cells = (Cell *)xrealloc(p->cells, (size_t)p->cell_cap * sizeof(Cell));
    }
    Cell *c = &p->cells[p->cell_count];
    snprintf(c->name, NAME_LEN, "%s", name);
    c->type = type;
    c->declared = declared;
    c->length = 0;
    c->is_function = 0;
    c->owner[0] = '\0';
    return p->cell_count++;
}

static void push_instr(TacProgram *p, const Instr *in)
{
    if (p->count == p->cap) {
        p->cap = p->cap ? p->cap * 2 : 64;
        p->code = (Instr *)xrealloc(p->code, (size_t)p->cap * sizeof(Instr));
    }
    p->code[p->count++] = *in;
}

/* int arithmetic is 32-bit, like the int registers in the assembly */
static long long wrap32(long long v) { return (long long)(int32_t)(uint32_t)(uint64_t)v; }

static Operand make_int(long long v)
{
    Operand o;
    memset(&o, 0, sizeof o);
    o.kind = OPD_INT; o.iv = wrap32(v); o.type = TAC_INT; o.slot = -1;
    return o;
}
static Operand make_float(double v)
{
    Operand o;
    memset(&o, 0, sizeof o);
    o.kind = OPD_FLOAT; o.fv = v; o.type = TAC_FLOAT; o.slot = -1;
    return o;
}
static Operand make_bool(int v)
{
    Operand o;
    memset(&o, 0, sizeof o);
    o.kind = OPD_BOOL; o.iv = v != 0; o.type = TAC_BOOL; o.slot = -1;
    return o;
}

/* A constant converted to the type of the variable it is stored in. */
static Operand convert_const(const Operand *c, TacType type)
{
    if (type == TAC_FLOAT && c->kind != OPD_FLOAT) return make_float((double)c->iv);
    if (type == TAC_INT && c->kind == OPD_FLOAT)   return make_int((long long)c->fv);
    if (type == TAC_BOOL && c->kind != OPD_BOOL)   return make_bool(c->kind == OPD_FLOAT ? c->fv != 0 : c->iv != 0);
    return *c;
}

/* ------------------------------------------------------------------ */
/* Text <-> instructions                                                */
/* ------------------------------------------------------------------ */

/* Float constants always keep a decimal point (3.0, not 3), like the front end. */
static void float_text(double v, char *buf, size_t size)
{
    snprintf(buf, size, "%g", v);
    if (strpbrk(buf, ".eEn") == NULL)    /* n: inf / nan */
        strncat(buf, ".0", size - strlen(buf) - 1);
}

static void operand_text(const Operand *o, char *buf, size_t size)
{
    switch (o->kind) {
    case OPD_NAME:  snprintf(buf, size, "%s", o->name); break;
    case OPD_INT:   snprintf(buf, size, "%ld", (long)o->iv); break;   /* fits: ints are 32-bit */
    case OPD_FLOAT: float_text(o->fv, buf, size); break;
    case OPD_BOOL:  snprintf(buf, size, "%s", o->iv ? "true" : "false"); break;
    default:        buf[0] = '\0';
    }
}

static void instr_text(const Instr *in, char *buf, size_t size)
{
    char r[NAME_LEN], a[NAME_LEN], b[NAME_LEN];
    operand_text(&in->result, r, sizeof r);
    operand_text(&in->a, a, sizeof a);
    operand_text(&in->b, b, sizeof b);
    switch (in->op) {
    case OP_ASSIGN:  snprintf(buf, size, "%s = %s", r, a); break;
    case OP_NEG:
    case OP_NOT:     snprintf(buf, size, "%s = %s %s", r, OP_SYMBOL[in->op], a); break;
    case OP_LABEL:   snprintf(buf, size, "%s:", in->label); break;
    case OP_GOTO:    snprintf(buf, size, "goto %s", in->label); break;
    case OP_IFFALSE: snprintf(buf, size, "ifFalse %s goto %s", a, in->label); break;
    case OP_IFTRUE:  snprintf(buf, size, "if %s goto %s", a, in->label); break;
    case OP_PRINT:   snprintf(buf, size, "print %s", a); break;
    case OP_READ:    snprintf(buf, size, "read %s", r); break;
    case OP_OUTS:    snprintf(buf, size, "outs \"%s\"", in->str); break;
    case OP_OUT:     snprintf(buf, size, "out %s", a); break;
    case OP_OUTF:    snprintf(buf, size, "outf %s %d", a, in->precision); break;
    case OP_LOAD:    snprintf(buf, size, "%s = %s[%s]", r, a, b); break;
    case OP_STORE:   snprintf(buf, size, "%s[%s] = %s", r, a, b); break;
    case OP_READA:   snprintf(buf, size, "read %s[%s]", r, a); break;
    case OP_FUNC:    snprintf(buf, size, "func %s:", in->label); break;
    case OP_FORMAL:  snprintf(buf, size, "formal %s", r); break;
    case OP_RETURN:  snprintf(buf, size, in->a.kind == OPD_NONE ? "return" : "return %s", a); break;
    case OP_ENDFUNC: snprintf(buf, size, "endfunc"); break;
    case OP_PARAM:   snprintf(buf, size, "param %s", a); break;
    case OP_CALL:
        if (in->result.kind == OPD_NAME) snprintf(buf, size, "%s = call %s %d", r, in->label, in->precision);
        else                             snprintf(buf, size, "call %s %d", in->label, in->precision);
        break;
    default:        snprintf(buf, size, "%s = %s %s %s", r, a, OP_SYMBOL[in->op], b); break;
    }
}

static int parse_operand(const char *token, Operand *o)
{
    memset(o, 0, sizeof *o);
    o->slot = -1;
    if (strlen(token) >= NAME_LEN)
        return 0;
    if (strcmp(token, "true") == 0 || strcmp(token, "false") == 0) {
        *o = make_bool(token[0] == 't');
        return 1;
    }
    const char *digits = token + (token[0] == '-' || token[0] == '+');
    if (isdigit((unsigned char)digits[0]) || (digits[0] == '.' && isdigit((unsigned char)digits[1]))) {
        char *end;
        if (strpbrk(token, ".eE") != NULL) *o = make_float(strtod(token, &end));
        else                               *o = make_int(strtoll(token, &end, 10));
        return *end == '\0';
    }
    if (!isalpha((unsigned char)token[0]) && token[0] != '_')
        return 0;
    o->kind = OPD_NAME;
    snprintf(o->name, NAME_LEN, "%s", token);
    return 1;
}

/* "a[i]" into the array name and the index operand. */
static int parse_element(const char *token, Operand *array, Operand *index)
{
    const char *open = strchr(token, '[');
    size_t len = strlen(token);
    if (open == NULL || open == token || len < 4 || token[len - 1] != ']')
        return 0;
    char name[NAME_LEN], inside[NAME_LEN];
    if ((size_t)(open - token) >= NAME_LEN || len - (size_t)(open - token) - 2 >= NAME_LEN)
        return 0;
    snprintf(name, sizeof name, "%.*s", (int)(open - token), token);
    snprintf(inside, sizeof inside, "%.*s", (int)(len - (size_t)(open - token) - 2), open + 1);
    return parse_operand(name, array) && array->kind == OPD_NAME && parse_operand(inside, index);
}

/* A function name for func / call (no brackets or other punctuation). */
static int copy_function_name(const char *token, char *label)
{
    if (strlen(token) >= NAME_LEN || (!isalpha((unsigned char)token[0]) && token[0] != '_'))
        return 0;
    snprintf(label, NAME_LEN, "%s", token);
    return 1;
}

static int binary_opcode(const char *symbol, OpCode *op)
{
    for (int k = OP_ADD; k <= OP_OR; k++)
        if (strcmp(symbol, OP_SYMBOL[k]) == 0) {
            *op = (OpCode)k;
            return 1;
        }
    return 0;
}

/* One TAC line into an instruction. Returns 0 if the line is not valid TAC. */
static int parse_line(char *line, Instr *in)
{
    char *tok[8];
    int n = 0;
    memset(in, 0, sizeof *in);
    in->result.slot = in->a.slot = in->b.slot = -1;

    /* outs "text": the text may contain spaces, so it is not split into tokens */
    char *start = line;
    while (*start == ' ' || *start == '\t') start++;
    if (strncmp(start, "outs ", 5) == 0) {
        char *open = strchr(start, '"');
        char *close = strrchr(start, '"');
        if (open == NULL || close == open)
            return 0;
        in->op = OP_OUTS;
        in->str = (char *)xalloc((size_t)(close - open));
        memcpy(in->str, open + 1, (size_t)(close - open - 1));
        in->str[close - open - 1] = '\0';
        return 1;
    }

    for (char *t = strtok(line, " \t\r"); t != NULL && n < 8; t = strtok(NULL, " \t\r"))
        tok[n++] = t;

    size_t len = n == 1 ? strlen(tok[0]) : 0;
    if (n == 1 && len > 1 && len < NAME_LEN && tok[0][len - 1] == ':') {
        in->op = OP_LABEL;
        snprintf(in->label, NAME_LEN, "%.*s", (int)(len - 1), tok[0]);
        return 1;
    }
    if (n == 2 && strcmp(tok[0], "goto") == 0 && strlen(tok[1]) < NAME_LEN) {
        in->op = OP_GOTO;
        snprintf(in->label, NAME_LEN, "%s", tok[1]);
        return 1;
    }
    if (n == 4 && (strcmp(tok[0], "ifFalse") == 0 || strcmp(tok[0], "if") == 0)
        && strcmp(tok[2], "goto") == 0 && strlen(tok[3]) < NAME_LEN) {
        in->op = tok[0][2] == 'F' ? OP_IFFALSE : OP_IFTRUE;
        snprintf(in->label, NAME_LEN, "%s", tok[3]);
        return parse_operand(tok[1], &in->a);
    }
    if (n == 2 && strcmp(tok[0], "print") == 0) {
        in->op = OP_PRINT;
        return parse_operand(tok[1], &in->a);
    }
    if (n == 2 && strcmp(tok[0], "read") == 0 && strchr(tok[1], '[') != NULL) {
        in->op = OP_READA;
        return parse_element(tok[1], &in->result, &in->a);
    }
    if (n == 2 && strcmp(tok[0], "read") == 0) {
        in->op = OP_READ;
        return parse_operand(tok[1], &in->result) && in->result.kind == OPD_NAME;
    }
    if (n == 2 && strcmp(tok[0], "func") == 0) {
        size_t flen = strlen(tok[1]);
        if (flen < 2 || tok[1][flen - 1] != ':')
            return 0;
        tok[1][flen - 1] = '\0';
        in->op = OP_FUNC;
        return copy_function_name(tok[1], in->label);
    }
    if (n == 1 && strcmp(tok[0], "endfunc") == 0) {
        in->op = OP_ENDFUNC;
        return 1;
    }
    if (n == 2 && strcmp(tok[0], "formal") == 0) {
        in->op = OP_FORMAL;
        return parse_operand(tok[1], &in->result) && in->result.kind == OPD_NAME;
    }
    if ((n == 1 || n == 2) && strcmp(tok[0], "return") == 0) {
        in->op = OP_RETURN;
        return n == 1 || parse_operand(tok[1], &in->a);
    }
    if (n == 2 && strcmp(tok[0], "param") == 0) {
        in->op = OP_PARAM;
        return parse_operand(tok[1], &in->a);
    }
    if ((n == 3 && strcmp(tok[0], "call") == 0) || (n == 5 && strcmp(tok[1], "=") == 0 && strcmp(tok[2], "call") == 0)) {
        char *end;
        in->op = OP_CALL;
        if (n == 5 && !(parse_operand(tok[0], &in->result) && in->result.kind == OPD_NAME))
            return 0;
        in->precision = (int)strtol(tok[n - 1], &end, 10);
        return *end == '\0' && in->precision >= 0 && copy_function_name(tok[n - 2], in->label);
    }
    if (n == 3 && strcmp(tok[1], "=") == 0 && strchr(tok[0], '[') != NULL) {
        in->op = OP_STORE;
        return parse_element(tok[0], &in->result, &in->a) && parse_operand(tok[2], &in->b);
    }
    if (n == 3 && strcmp(tok[1], "=") == 0 && strchr(tok[2], '[') != NULL) {
        in->op = OP_LOAD;
        return parse_operand(tok[0], &in->result) && in->result.kind == OPD_NAME
            && parse_element(tok[2], &in->a, &in->b);
    }
    if (n == 2 && strcmp(tok[0], "out") == 0) {
        in->op = OP_OUT;
        return parse_operand(tok[1], &in->a);
    }
    if (n == 3 && strcmp(tok[0], "outf") == 0) {
        char *end;
        in->op = OP_OUTF;
        in->precision = (int)strtol(tok[2], &end, 10);
        return *end == '\0' && in->precision >= 0 && in->precision <= 20 && parse_operand(tok[1], &in->a);
    }
    if (n >= 3 && n <= 5 && strcmp(tok[1], "=") == 0) {
        if (!parse_operand(tok[0], &in->result) || in->result.kind != OPD_NAME)
            return 0;
        if (n == 3) {
            in->op = OP_ASSIGN;
            return parse_operand(tok[2], &in->a);
        }
        if (n == 4) {
            if (strcmp(tok[2], "-") == 0)      in->op = OP_NEG;
            else if (strcmp(tok[2], "!") == 0) in->op = OP_NOT;
            else return 0;
            return parse_operand(tok[3], &in->a);
        }
        return binary_opcode(tok[3], &in->op) && parse_operand(tok[2], &in->a)
            && parse_operand(tok[4], &in->b);
    }
    return 0;
}

/* Type of an operand; a name gets its cell (a name used before any
 * definition, which the front end never writes, is taken as an int). */
static TacType operand_type(TacProgram *p, Operand *o)
{
    if (o->kind != OPD_NAME)
        return o->type;
    int k = cell_find(p, o->name);
    if (k < 0)
        k = cell_add(p, o->name, TAC_INT, 0);
    o->slot = k;
    o->type = p->cells[k].type;
    return o->type;
}

/* Works out the type of every operation and temporary, in program order
 * (a temporary is always defined before it is used). */
static void resolve_types(TacProgram *p)
{
    const char *function = "";      /* the func ... endfunc block we are in */
    for (int i = 0; i < p->count; i++) {
        Instr *in = &p->code[i];
        TacType a = in->a.kind != OPD_NONE ? operand_type(p, &in->a) : TAC_INT;
        TacType b = in->b.kind != OPD_NONE ? operand_type(p, &in->b) : TAC_INT;
        TacType value = a;    /* type of the value the instruction produces */

        if (in->op >= OP_ADD && in->op <= OP_MOD) {
            in->ty = (a == TAC_FLOAT || b == TAC_FLOAT) ? TAC_FLOAT : TAC_INT;
            value = in->ty;
        } else if (is_compare(in->op)) {
            in->ty = (a == TAC_FLOAT || b == TAC_FLOAT) ? TAC_FLOAT
                   : (a == TAC_BOOL && b == TAC_BOOL) ? TAC_BOOL : TAC_INT;
            value = TAC_BOOL;
        } else if (in->op == OP_AND || in->op == OP_OR || in->op == OP_NOT) {
            in->ty = TAC_BOOL;
            value = TAC_BOOL;
        } else if (in->op == OP_CALL) {
            int f = cell_find(p, in->label);     /* the value has the function's return type */
            value = in->ty = f >= 0 ? p->cells[f].type : TAC_INT;
        } else {
            in->ty = a;       /* assign, negate, print, if, a[i] (the element type) */
        }

        if (in->op == OP_FUNC)                  /* temporaries defined from here on are its locals */
            function = in->label;
        else if (in->op == OP_ENDFUNC)
            function = "";

        if (in->op == OP_STORE || in->op == OP_READA) {   /* the array */
            operand_type(p, &in->result);
            in->ty = in->result.type;           /* the value converts to the element type */
        }
        if (defines_result(in)) {
            int k = cell_find(p, in->result.name);
            if (k < 0) {
                k = cell_add(p, in->result.name, value, 0);
                snprintf(p->cells[k].owner, NAME_LEN, "%s", function);
            } else if (!p->cells[k].declared) {
                p->cells[k].type = value;
            }
            in->result.slot = k;
            in->result.type = p->cells[k].type;
            if (in->op == OP_ASSIGN || in->op == OP_READ || in->op == OP_FORMAL)
                in->ty = in->result.type;   /* the copy converts to the variable's type */
        }
    }
}

TacProgram *tac_build(const char *text, const TacVar *vars, int var_count,
                      char *error, int error_size)
{
    TacProgram *p = (TacProgram *)xalloc(sizeof(TacProgram));
    memset(p, 0, sizeof *p);
    for (int k = 0; k < var_count; k++)
        if (cell_find(p, vars[k].name) < 0) {
            int slot = cell_add(p, vars[k].name, vars[k].type, 1);   /* may move p->cells */
            Cell *c = &p->cells[slot];
            c->length = vars[k].length > 0 ? vars[k].length : 0;
            c->is_function = vars[k].is_function;
            if (vars[k].function != NULL)
                snprintf(c->owner, NAME_LEN, "%s", vars[k].function);
        }

    const char *start = text;
    int line_number = 0;
    while (*start != '\0') {
        const char *end = strchr(start, '\n');
        size_t len = end ? (size_t)(end - start) : strlen(start);
        char *line = (char *)xalloc(len + 1);
        memcpy(line, start, len);
        line[len] = '\0';
        line_number++;

        int blank = 1;
        for (size_t k = 0; k < len; k++)
            if (!isspace((unsigned char)line[k])) blank = 0;
        if (!blank) {
            char copy[512];
            snprintf(copy, sizeof copy, "%s", line);
            Instr in;
            if (!parse_line(line, &in)) {
                if (error != NULL)
                    snprintf(error, (size_t)error_size, "cannot read TAC line %d: %s", line_number, copy);
                free(line);
                tac_free(p);
                return NULL;
            }
            push_instr(p, &in);
        }
        free(line);
        start = end ? end + 1 : start + len;
    }
    resolve_types(p);
    return p;
}

void tac_free(TacProgram *p)
{
    if (p == NULL)
        return;
    for (int i = 0; i < p->count; i++)
        free(p->code[i].str);
    free(p->code);
    free(p->cells);
    free(p);
}

char *tac_text(const TacProgram *p)
{
    Text t = { NULL, 0, 0 };
    char line[3 * NAME_LEN + 32];
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op == OP_OUTS) {           /* the text can be longer than a line buffer */
            text_add(&t, "outs \"%s\"\n", p->code[i].str);
            continue;
        }
        instr_text(&p->code[i], line, sizeof line);
        text_add(&t, "%s\n", line);
    }
    return text_take(&t);
}

/* ------------------------------------------------------------------ */
/* Optimizer                                                            */
/* ------------------------------------------------------------------ */

static double number(const Operand *o) { return o->kind == OPD_FLOAT ? o->fv : (double)o->iv; }

/* Computes an operation on constant operands. Returns 0 when it cannot be
 * done at compile time (division by zero, int overflow of INT_MIN / -1). */
static int fold(const Instr *in, Operand *out)
{
    const Operand *a = &in->a, *b = &in->b;
    if (in->op == OP_NOT) { *out = make_bool(!a->iv); return 1; }
    if (in->op == OP_NEG) {
        *out = a->kind == OPD_FLOAT ? make_float(-a->fv) : make_int(-a->iv);
        return 1;
    }
    if (!is_binary(in->op))
        return 0;
    if (in->op == OP_AND) { *out = make_bool(a->iv && b->iv); return 1; }
    if (in->op == OP_OR)  { *out = make_bool(a->iv || b->iv); return 1; }
    if (in->ty == TAC_FLOAT) {
        double x = number(a), y = number(b);
        switch (in->op) {
        case OP_ADD: *out = make_float(x + y); return 1;
        case OP_SUB: *out = make_float(x - y); return 1;
        case OP_MUL: *out = make_float(x * y); return 1;
        case OP_DIV: if (y == 0) return 0; *out = make_float(x / y); return 1;
        case OP_LT:  *out = make_bool(x < y);  return 1;
        case OP_GT:  *out = make_bool(x > y);  return 1;
        case OP_LE:  *out = make_bool(x <= y); return 1;
        case OP_GE:  *out = make_bool(x >= y); return 1;
        case OP_EQ:  *out = make_bool(x == y); return 1;
        case OP_NE:  *out = make_bool(x != y); return 1;
        default:     return 0;
        }
    }
    long long x = a->iv, y = b->iv;
    switch (in->op) {
    case OP_ADD: *out = make_int(x + y); return 1;
    case OP_SUB: *out = make_int(x - y); return 1;
    case OP_MUL: *out = make_int(x * y); return 1;
    case OP_DIV:
    case OP_MOD:
        if (y == 0 || (x == INT32_MIN && y == -1)) return 0;
        *out = make_int(in->op == OP_DIV ? x / y : x % y);
        return 1;
    case OP_LT:  *out = make_bool(x < y);  return 1;
    case OP_GT:  *out = make_bool(x > y);  return 1;
    case OP_LE:  *out = make_bool(x <= y); return 1;
    case OP_GE:  *out = make_bool(x >= y); return 1;
    case OP_EQ:  *out = make_bool(x == y); return 1;
    case OP_NE:  *out = make_bool(x != y); return 1;
    default:     return 0;
    }
}

/* Pass 1: constant propagation, folding and branch simplification inside each
 * basic block (what is known about variables is forgotten at every label). */
static void fold_pass(TacProgram *p)
{
    int cells = p->cell_count;
    Operand *value = (Operand *)xalloc((size_t)(cells ? cells : 1) * sizeof(Operand));
    char *known = (char *)xalloc((size_t)(cells ? cells : 1));
    memset(known, 0, (size_t)(cells ? cells : 1));
    int n = 0;
    for (int i = 0; i < p->count; i++) {
        Instr in = p->code[i];
        if (in.op == OP_LABEL || in.op == OP_FUNC || in.op == OP_ENDFUNC) {
            memset(known, 0, (size_t)(cells ? cells : 1));
            p->code[n++] = in;
            continue;
        }
        if (in.a.kind == OPD_NAME && known[in.a.slot]) in.a = value[in.a.slot];
        if (in.b.kind == OPD_NAME && known[in.b.slot]) in.b = value[in.b.slot];

        if ((in.op == OP_IFFALSE || in.op == OP_IFTRUE) && is_const(&in.a)) {
            int jumps = in.op == OP_IFTRUE ? in.a.iv != 0 : in.a.iv == 0;
            if (jumps) {          /* always taken: an unconditional jump */
                in.op = OP_GOTO;
                memset(&in.a, 0, sizeof in.a);
                p->code[n++] = in;
            }
            continue;             /* never taken: dropped */
        }
        if (defines_result(&in)) {
            Operand folded;
            int b_const = in.b.kind == OPD_NONE || is_const(&in.b);
            if (in.op != OP_ASSIGN && is_const(&in.a) && b_const && fold(&in, &folded)) {
                in.op = OP_ASSIGN;
                in.a = folded;
                memset(&in.b, 0, sizeof in.b);
            }
            if (in.op == OP_ASSIGN && is_const(&in.a)) {
                value[in.result.slot] = convert_const(&in.a, in.result.type);
                known[in.result.slot] = 1;
            } else {
                known[in.result.slot] = 0;
            }
        }
        if (in.op == OP_CALL)          /* the function may change any global variable */
            memset(known, 0, (size_t)(cells ? cells : 1));
        p->code[n++] = in;
    }
    p->count = n;
    free(value);
    free(known);
}

/* Pass 2: code after an unconditional jump (or a return) is unreachable
 * until the next label (or the start or end of a function). */
static void unreachable_pass(TacProgram *p)
{
    int n = 0, dead = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op == OP_LABEL || p->code[i].op == OP_FUNC || p->code[i].op == OP_ENDFUNC)
            dead = 0;
        if (dead) {
            free(p->code[i].str);
            continue;
        }
        p->code[n++] = p->code[i];
        if (p->code[i].op == OP_GOTO || p->code[i].op == OP_RETURN) dead = 1;
    }
    p->count = n;
}

/* Pass 3: temporaries that are never used are removed, and
 * "t = a op b; x = t" becomes "x = a op b" when t is used only there. */
static void dead_temp_pass(TacProgram *p)
{
    int cells = p->cell_count ? p->cell_count : 1;
    int *uses = (int *)xalloc((size_t)cells * sizeof(int));
    int *defs = (int *)xalloc((size_t)cells * sizeof(int));
    memset(uses, 0, (size_t)cells * sizeof(int));
    memset(defs, 0, (size_t)cells * sizeof(int));
    for (int i = 0; i < p->count; i++) {
        const Instr *in = &p->code[i];
        if (in->a.kind == OPD_NAME) uses[in->a.slot]++;
        if (in->b.kind == OPD_NAME) uses[in->b.slot]++;
        if (defines_result(in)) defs[in->result.slot]++;
    }
    int n = 0;
    for (int i = 0; i < p->count; i++) {
        Instr in = p->code[i];
        int temp = defines_result(&in) && !p->cells[in.result.slot].declared;
        if (temp && uses[in.result.slot] == 0 && in.op == OP_CALL) {
            memset(&in.result, 0, sizeof in.result);     /* the call stays: it may print */
            in.result.slot = -1;
        } else if (temp && uses[in.result.slot] == 0) {
            continue;
        }
        if (in.op == OP_ASSIGN && in.a.kind == OPD_NAME && !p->cells[in.a.slot].declared
            && uses[in.a.slot] == 1 && defs[in.a.slot] == 1 && n > 0) {
            Instr *prev = &p->code[n - 1];
            if (defines_result(prev) && prev->result.slot == in.a.slot
                && prev->result.type == in.result.type) {
                prev->result = in.result;
                continue;
            }
        }
        p->code[n++] = in;
    }
    p->count = n;
    free(uses);
    free(defs);
}

/* Pass 4: "goto L; L:" loses the goto, and labels nobody jumps to are removed. */
static void jump_pass(TacProgram *p)
{
    int n = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op == OP_GOTO && i + 1 < p->count && p->code[i + 1].op == OP_LABEL
            && strcmp(p->code[i].label, p->code[i + 1].label) == 0)
            continue;
        p->code[n++] = p->code[i];
    }
    p->count = n;
    n = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op == OP_LABEL) {
            int used = 0;
            for (int j = 0; j < p->count && !used; j++)
                used = (p->code[j].op == OP_GOTO || p->code[j].op == OP_IFFALSE
                        || p->code[j].op == OP_IFTRUE)
                       && strcmp(p->code[j].label, p->code[i].label) == 0;
            if (!used) continue;
        }
        p->code[n++] = p->code[i];
    }
    p->count = n;
}

void tac_optimize(TacProgram *p)
{
    for (int round = 0; round < 20; round++) {      /* repeat until nothing changes */
        char *before = tac_text(p);
        fold_pass(p);
        unreachable_pass(p);
        dead_temp_pass(p);
        jump_pass(p);
        resolve_types(p);
        char *after = tac_text(p);
        int same = strcmp(before, after) == 0;
        free(before);
        free(after);
        if (same) break;
    }
}

/* ------------------------------------------------------------------ */
/* Interpreter                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    TacType ty;
    long long iv;    /* int, or 0/1 for bool */
    double fv;       /* float */
} Value;

static Value const_value(const Operand *o)
{
    Value v;
    v.ty = o->type; v.iv = o->iv; v.fv = o->fv;
    return v;
}

static double as_double(Value v) { return v.ty == TAC_FLOAT ? v.fv : (double)v.iv; }

/* Stores into a declared variable converting to its type, like the
 * assignment in the source program. */
static Value convert_value(Value v, TacType type)
{
    Value r;
    r.ty = type; r.iv = 0; r.fv = 0;
    if (type == TAC_FLOAT)     r.fv = as_double(v);
    else if (type == TAC_BOOL) r.iv = v.ty == TAC_FLOAT ? v.fv != 0 : v.iv != 0;
    else                       r.iv = v.ty == TAC_FLOAT ? (long long)v.fv : v.iv;
    return r;
}

/* The characters a C string literal stands for: \n becomes a newline, etc. */
static char *unescape(const char *s)
{
    char *out = (char *)xalloc(strlen(s) + 1);
    size_t n = 0;
    for (; *s != '\0'; s++) {
        if (*s != '\\' || s[1] == '\0') { out[n++] = *s; continue; }
        s++;
        switch (*s) {
        case 'n':  out[n++] = '\n'; break;
        case 't':  out[n++] = '\t'; break;
        case 'r':  out[n++] = '\r'; break;
        case '0':  break;                      /* \0 would end the string: nothing printed after it */
        default:   out[n++] = *s;   break;     /* \\ \" \' */
        }
    }
    out[n] = '\0';
    return out;
}

static int label_index(const TacProgram *p, const char *label)
{
    for (int i = 0; i < p->count; i++)
        if (p->code[i].op == OP_LABEL && strcmp(p->code[i].label, label) == 0)
            return i;
    return -1;
}

/* read x: the next word (separated by spaces or newlines) of the standard
 * input, as a value of x's type. A missing or wrong word stops the program
 * with a runtime error, like Java's Scanner. */
static void read_value(TacType type, Value *r, const char **runtime_error)
{
    static char message[160];
    const char *want = type == TAC_FLOAT ? "a float" : type == TAC_BOOL ? "a bool (true or false)" : "an int";
    char word[64], *end;
    r->ty = type; r->iv = 0; r->fv = 0;
    if (scanf("%63s", word) != 1) {
        snprintf(message, sizeof message, "Runtime error: no more input (the program wanted %s)", want);
        *runtime_error = message;
        return;
    }
    int ok;
    if (type == TAC_FLOAT) {
        r->fv = strtod(word, &end);
        ok = end != word && *end == '\0';
    } else if (type == TAC_BOOL && (strcmp(word, "true") == 0 || strcmp(word, "false") == 0)) {
        r->iv = word[0] == 't';
        ok = 1;
    } else {
        long long v = strtoll(word, &end, 10);
        ok = end != word && *end == '\0';
        r->iv = type == TAC_BOOL ? v != 0 : wrap32(v);
    }
    if (!ok) {
        snprintf(message, sizeof message, "Runtime error: the input '%s' is not %s", word, want);
        *runtime_error = message;
    }
}

/* Index of the instruction `func name:`, or -1. */
static int function_index(const TacProgram *p, const char *name)
{
    for (int i = 0; i < p->count; i++)
        if (p->code[i].op == OP_FUNC && strcmp(p->code[i].label, name) == 0)
            return i;
    return -1;
}

/* One active call: where to go back to and the caller's copy of the
 * callee's locals (restored on return, so recursion works). */
typedef struct {
    int return_pc;
    int result_slot;      /* cell for the returned value, or -1 */
    int function_cell;    /* cell of the function name: its return type */
    int owner;            /* index of the function in the program's function list */
    Value *saved;         /* saved values of the callee's locals, arrays included */
    Value *args;          /* the arguments, taken by formal in order */
    int arg_count, next_arg;
} Frame;

/* The locals of each function: the cells it owns. */
typedef struct {
    char name[NAME_LEN];
    int *slots;
    int slot_count;
    int value_count;      /* scalars plus every array element */
} FunctionLocals;

static int function_find(const FunctionLocals *fl, int count, const char *name)
{
    for (int k = 0; k < count; k++)
        if (strcmp(fl[k].name, name) == 0)
            return k;
    return -1;
}

/* Builds the list of functions with the cells each one owns. */
static FunctionLocals *function_locals(const TacProgram *p, int *count)
{
    int n = 0;
    for (int i = 0; i < p->count; i++)
        if (p->code[i].op == OP_FUNC) n++;
    FunctionLocals *fl = (FunctionLocals *)xalloc((size_t)(n ? n : 1) * sizeof(FunctionLocals));
    n = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op != OP_FUNC)
            continue;
        FunctionLocals *f = &fl[n++];
        snprintf(f->name, NAME_LEN, "%s", p->code[i].label);
        f->slots = (int *)xalloc((size_t)(p->cell_count ? p->cell_count : 1) * sizeof(int));
        f->slot_count = 0;
        f->value_count = 0;
        for (int k = 0; k < p->cell_count; k++)
            if (!p->cells[k].is_function && strcmp(p->cells[k].owner, f->name) == 0) {
                f->slots[f->slot_count++] = k;
                f->value_count += p->cells[k].length > 0 ? p->cells[k].length : 1;
            }
    }
    *count = n;
    return fl;
}

static void function_locals_free(FunctionLocals *fl, int count)
{
    for (int k = 0; k < count; k++)
        free(fl[k].slots);
    free(fl);
}

char *tac_run(const TacProgram *p, const char **runtime_error)
{
    static char message[200];
    Text out = { NULL, 0, 0 };
    *runtime_error = NULL;
    int cells = p->cell_count ? p->cell_count : 1;
    Value *mem = (Value *)xalloc((size_t)cells * sizeof(Value));
    Value **arrays = (Value **)xalloc((size_t)cells * sizeof(Value *));
    for (int k = 0; k < p->cell_count; k++) {     /* every variable starts at 0 */
        mem[k].ty = p->cells[k].type; mem[k].iv = 0; mem[k].fv = 0;
        arrays[k] = NULL;
        if (p->cells[k].length > 0) {
            arrays[k] = (Value *)xalloc((size_t)p->cells[k].length * sizeof(Value));
            for (int e = 0; e < p->cells[k].length; e++)
                arrays[k][e] = mem[k];
        }
    }
    int *target = (int *)xalloc((size_t)(p->count ? p->count : 1) * sizeof(int));
    for (int i = 0; i < p->count; i++) {
        const Instr *in = &p->code[i];
        target[i] = -1;
        if (is_jump(in->op))
            target[i] = label_index(p, in->label);
        else if (in->op == OP_CALL)
            target[i] = function_index(p, in->label);
        else if (in->op == OP_FUNC) {             /* skipped when reached: jump past endfunc */
            int j = i + 1;
            while (j < p->count && p->code[j].op != OP_ENDFUNC) j++;
            target[i] = j;
        }
    }
    int function_count;
    FunctionLocals *locals = function_locals(p, &function_count);
    Frame *frames = (Frame *)xalloc(MAX_CALLS * sizeof(Frame));
    int depth = 0;
    Value *pending = NULL;              /* arguments passed with param, not yet called */
    int pending_count = 0, pending_cap = 0;

    long steps = 0;
    for (int pc = 0; pc < p->count && *runtime_error == NULL; pc++) {
        if (++steps > MAX_STEPS) { *runtime_error = "Runtime error: step limit exceeded (endless loop?)"; break; }
        if (out.len > MAX_OUTPUT) { *runtime_error = "Runtime error: output limit exceeded (1 MB)"; break; }
        const Instr *in = &p->code[pc];
        Value a, b, r;
        a = in->a.kind == OPD_NAME ? mem[in->a.slot] : const_value(&in->a);
        b = in->b.kind == OPD_NAME ? mem[in->b.slot] : const_value(&in->b);
        r.ty = TAC_INT; r.iv = 0; r.fv = 0;

        switch (in->op) {
        case OP_LABEL:
            continue;
        case OP_GOTO:
            if (target[pc] < 0) { *runtime_error = "Runtime error: jump to an unknown label"; break; }
            pc = target[pc];
            continue;
        case OP_IFFALSE:
        case OP_IFTRUE: {
            int truth = a.ty == TAC_FLOAT ? a.fv != 0 : a.iv != 0;
            if ((in->op == OP_IFTRUE) == truth) {
                if (target[pc] < 0) { *runtime_error = "Runtime error: jump to an unknown label"; break; }
                pc = target[pc];
            }
            continue;
        }
        case OP_PRINT:
            if (a.ty == TAC_FLOAT)     text_add(&out, "%g\n", a.fv);
            else if (a.ty == TAC_BOOL) text_add(&out, "%s\n", a.iv ? "true" : "false");
            else                       text_add(&out, "%ld\n", (long)a.iv);
            continue;
        case OP_OUTS: {                   /* printf text */
            char *text = unescape(in->str);
            text_add(&out, "%s", text);
            free(text);
            continue;
        }
        case OP_OUT:                      /* printf %d / %g: like C, a bool prints as 0 or 1 */
            if (a.ty == TAC_FLOAT) text_add(&out, "%g", a.fv);
            else                   text_add(&out, "%ld", (long)a.iv);
            continue;
        case OP_OUTF:                     /* printf %f / %.Nf */
            text_add(&out, "%.*f", in->precision, as_double(a));
            continue;
        case OP_READ:
            read_value(p->cells[in->result.slot].type, &r, runtime_error);
            break;

        /* ----- arrays ----- */
        case OP_LOAD:
        case OP_STORE:
        case OP_READA: {
            int array = in->op == OP_LOAD ? in->a.slot : in->result.slot;
            Value index = in->op == OP_LOAD ? b : a;
            const Cell *c = &p->cells[array];
            if (arrays[array] == NULL || index.iv < 0 || index.iv >= c->length) {
                snprintf(message, sizeof message, "Runtime error: index %ld is out of bounds for array '%s' of size %d",
                         (long)index.iv, c->name, c->length);
                *runtime_error = message;
                break;
            }
            if (in->op == OP_LOAD) {
                r = arrays[array][index.iv];
                break;                    /* stored into the result below */
            }
            if (in->op == OP_READA) {
                read_value(c->type, &r, runtime_error);
                arrays[array][index.iv] = r;
            } else {
                arrays[array][index.iv] = convert_value(b, c->type);
            }
            continue;
        }

        /* ----- functions ----- */
        case OP_FUNC:                     /* reached without a call: skip the function */
            pc = target[pc];
            continue;
        case OP_PARAM:
            if (pending_count == pending_cap) {
                pending_cap = pending_cap ? pending_cap * 2 : 16;
                pending = (Value *)xrealloc(pending, (size_t)pending_cap * sizeof(Value));
            }
            pending[pending_count++] = a;
            continue;
        case OP_CALL: {
            int owner = function_find(locals, function_count, in->label);
            if (target[pc] < 0 || owner < 0) {
                snprintf(message, sizeof message, "Runtime error: call of an unknown function '%s'", in->label);
                *runtime_error = message;
                break;
            }
            if (depth == MAX_CALLS) {
                *runtime_error = "Runtime error: too many nested calls (endless recursion?)";
                break;
            }
            if (in->precision > pending_count) {
                *runtime_error = "Runtime error: call with missing arguments";
                break;
            }
            Frame *f = &frames[depth++];
            const FunctionLocals *fl = &locals[owner];
            f->return_pc = pc;
            f->result_slot = in->result.kind == OPD_NAME ? in->result.slot : -1;
            f->function_cell = cell_find(p, in->label);
            f->owner = owner;
            f->arg_count = in->precision;
            f->next_arg = 0;
            f->args = (Value *)xalloc((size_t)(f->arg_count ? f->arg_count : 1) * sizeof(Value));
            memcpy(f->args, pending + pending_count - f->arg_count, (size_t)f->arg_count * sizeof(Value));
            pending_count -= f->arg_count;
            /* save the callee's locals, then start them at 0 */
            f->saved = (Value *)xalloc((size_t)(fl->value_count ? fl->value_count : 1) * sizeof(Value));
            int v = 0;
            for (int k = 0; k < fl->slot_count; k++) {
                int slot = fl->slots[k];
                Value zero;
                zero.ty = p->cells[slot].type; zero.iv = 0; zero.fv = 0;
                if (arrays[slot] != NULL) {
                    for (int e = 0; e < p->cells[slot].length; e++) {
                        f->saved[v++] = arrays[slot][e];
                        arrays[slot][e] = zero;
                    }
                } else {
                    f->saved[v++] = mem[slot];
                    mem[slot] = zero;
                }
            }
            pc = target[pc];              /* continues after `func f:` */
            continue;
        }
        case OP_FORMAL:
            if (depth == 0 || frames[depth - 1].next_arg >= frames[depth - 1].arg_count) {
                *runtime_error = "Runtime error: a function was called with too few arguments";
                break;
            }
            r = frames[depth - 1].args[frames[depth - 1].next_arg++];
            break;                        /* stored (converted) into the parameter below */
        case OP_RETURN:
        case OP_ENDFUNC: {
            if (depth == 0) {
                *runtime_error = "Runtime error: return outside of a function call";
                break;
            }
            Frame *f = &frames[--depth];
            const FunctionLocals *fl = &locals[f->owner];
            TacType type = f->function_cell >= 0 ? p->cells[f->function_cell].type : TAC_INT;
            Value result = convert_value(in->op == OP_RETURN && in->a.kind != OPD_NONE ? a : r, type);
            int v = 0;
            for (int k = 0; k < fl->slot_count; k++) {     /* the caller's locals come back */
                int slot = fl->slots[k];
                if (arrays[slot] != NULL)
                    for (int e = 0; e < p->cells[slot].length; e++)
                        arrays[slot][e] = f->saved[v++];
                else
                    mem[slot] = f->saved[v++];
            }
            free(f->saved);
            free(f->args);
            pc = f->return_pc;
            if (f->result_slot >= 0) {
                int slot = f->result_slot;
                mem[slot] = p->cells[slot].declared ? convert_value(result, p->cells[slot].type) : result;
            }
            continue;
        }

        case OP_ASSIGN:
            r = a;
            break;
        case OP_NEG:
            r.ty = a.ty;
            if (a.ty == TAC_FLOAT) r.fv = -a.fv; else r.iv = wrap32(-a.iv);
            break;
        case OP_NOT:
            r.ty = TAC_BOOL; r.iv = !a.iv;
            break;
        default:
            if (in->op == OP_AND || in->op == OP_OR) {
                r.ty = TAC_BOOL;
                r.iv = in->op == OP_AND ? (a.iv && b.iv) : (a.iv || b.iv);
            } else if (in->ty == TAC_FLOAT) {
                double x = as_double(a), y = as_double(b);
                r.ty = is_compare(in->op) ? TAC_BOOL : TAC_FLOAT;
                switch (in->op) {
                case OP_ADD: r.fv = x + y; break;
                case OP_SUB: r.fv = x - y; break;
                case OP_MUL: r.fv = x * y; break;
                case OP_DIV:
                    if (y == 0) { *runtime_error = "Runtime error: division by zero"; break; }
                    r.fv = x / y; break;
                case OP_LT: r.iv = x < y;  break;
                case OP_GT: r.iv = x > y;  break;
                case OP_LE: r.iv = x <= y; break;
                case OP_GE: r.iv = x >= y; break;
                case OP_EQ: r.iv = x == y; break;
                case OP_NE: r.iv = x != y; break;
                default: break;
                }
            } else {
                long long x = a.iv, y = b.iv;
                r.ty = is_compare(in->op) ? TAC_BOOL : TAC_INT;
                switch (in->op) {
                case OP_ADD: r.iv = wrap32(x + y); break;
                case OP_SUB: r.iv = wrap32(x - y); break;
                case OP_MUL: r.iv = wrap32(x * y); break;
                case OP_DIV:
                case OP_MOD:
                    if (y == 0) {
                        *runtime_error = in->op == OP_DIV ? "Runtime error: division by zero"
                                                          : "Runtime error: modulo by zero";
                        break;
                    }
                    if (x == INT32_MIN && y == -1) r.iv = in->op == OP_DIV ? x : 0;
                    else r.iv = in->op == OP_DIV ? x / y : x % y;
                    break;
                case OP_LT: r.iv = x < y;  break;
                case OP_GT: r.iv = x > y;  break;
                case OP_LE: r.iv = x <= y; break;
                case OP_GE: r.iv = x >= y; break;
                case OP_EQ: r.iv = x == y; break;
                case OP_NE: r.iv = x != y; break;
                default: break;
                }
            }
        }
        if (*runtime_error != NULL)
            break;
        int slot = in->result.slot;
        mem[slot] = p->cells[slot].declared ? convert_value(r, p->cells[slot].type) : r;
    }
    while (depth > 0) {                   /* stopped inside a call */
        depth--;
        free(frames[depth].saved);
        free(frames[depth].args);
    }
    for (int k = 0; k < p->cell_count; k++)
        free(arrays[k]);
    free(arrays);
    free(frames);
    free(pending);
    function_locals_free(locals, function_count);
    free(mem);
    free(target);
    return text_take(&out);
}

/* ------------------------------------------------------------------ */
/* x86 assembly (32-bit, AT&T syntax, MinGW gcc), in the style of the   */
/* C++ compiler: every variable and temporary lives in memory, int and  */
/* bool values go through %eax/%ecx, float values through the x87 stack */
/* ------------------------------------------------------------------ */

typedef struct {
    Text text;            /* the code; the data section is added in front */
    double *floats;       /* float constant pool .LF0, .LF1, ... */
    int float_count, float_cap;
    int print_labels;
    int uses_read;        /* a read was emitted: add the scanf formats to .data */
    const char **strings; /* printf text pool .LS0, .LS1, ... (C escapes kept) */
    int string_count, string_cap;
    int uses_printf;      /* a printf piece was emitted: add its formats to .data */
    const TacProgram *program;
    int pc;               /* index of the instruction being translated */
    const char *function; /* the function being translated ("" in main) */
    int formal_index;     /* formal instructions seen in this function so far */
} Asm;

static int string_label(Asm *g, const char *text)
{
    if (g->string_count == g->string_cap) {
        g->string_cap = g->string_cap ? g->string_cap * 2 : 16;
        g->strings = (const char **)xrealloc(g->strings, (size_t)g->string_cap * sizeof(char *));
    }
    g->strings[g->string_count] = text;
    return g->string_count++;
}

static void asm_ins(Asm *g, const char *op, const char *a, const char *b);
static void int_operand(const Operand *o, char *buf, size_t size);
static void load_float(Asm *g, const Operand *o);

/* A printf piece: pushes the arguments and calls printf. */
static void emit_out(Asm *g, const Instr *in)
{
    char a[NAME_LEN + 32];
    g->uses_printf = 1;
    if (in->op == OP_OUTS) {
        snprintf(a, sizeof a, "$.LS%d", string_label(g, in->str));
        asm_ins(g, "pushl", a, NULL);
        asm_ins(g, "pushl", "$.Lfmt_str_n", NULL);
        asm_ins(g, "call", "_printf", NULL);
        asm_ins(g, "addl", "$8", "%esp");
        return;
    }
    if (in->op == OP_OUTF || in->a.type == TAC_FLOAT) {   /* printf takes a double */
        asm_ins(g, "subl", "$16", "%esp");
        if (in->op == OP_OUTF) {
            snprintf(a, sizeof a, "$%d", in->precision);
            asm_ins(g, "movl", "$.Lfmt_prec_n", "(%esp)");
            asm_ins(g, "movl", a, "4(%esp)");
            load_float(g, &in->a);
            asm_ins(g, "fstpl", "8(%esp)", NULL);
        } else {
            asm_ins(g, "movl", "$.Lfmt_float_n", "(%esp)");
            load_float(g, &in->a);
            asm_ins(g, "fstpl", "4(%esp)", NULL);
        }
        asm_ins(g, "call", "_printf", NULL);
        asm_ins(g, "addl", "$16", "%esp");
        return;
    }
    int_operand(&in->a, a, sizeof a);              /* int, and bool as 0/1 */
    asm_ins(g, "pushl", a, NULL);
    asm_ins(g, "pushl", "$.Lfmt_int_n", NULL);
    asm_ins(g, "call", "_printf", NULL);
    asm_ins(g, "addl", "$8", "%esp");
}

/* The string as GNU as wants it in .asciz "...": C escapes become
 * \n \t \r \\ \" and anything else unusual an octal escape. */
static void add_asm_string(Text *out, const char *escaped)
{
    char *raw = unescape(escaped);
    text_add(out, "    .asciz \"");
    for (const unsigned char *c = (const unsigned char *)raw; *c != '\0'; c++) {
        if (*c == '\n')                 text_add(out, "\\n");
        else if (*c == '\t')            text_add(out, "\\t");
        else if (*c == '\r')            text_add(out, "\\r");
        else if (*c == '\\' || *c == '"') text_add(out, "\\%c", *c);
        else if (*c < 32 || *c > 126)   text_add(out, "\\%03o", *c);
        else                            text_add(out, "%c", *c);
    }
    text_add(out, "\"\n");
    free(raw);
}

static void asm_ins(Asm *g, const char *op, const char *a, const char *b)
{
    if (b != NULL)      text_add(&g->text, "    %-8s%s, %s\n", op, a, b);
    else if (a != NULL) text_add(&g->text, "    %-8s%s\n", op, a);
    else                text_add(&g->text, "    %s\n", op);
}

/* Memory cell of a variable: _v_name ('@' of a shadowing x@3 becomes '.'). */
static void cell_label(const char *name, char *buf, size_t size)
{
    snprintf(buf, size, "_v_%s", name);
    for (char *c = buf; *c != '\0'; c++)
        if (*c == '@') *c = '.';
}

static void float_label(Asm *g, double v, char *buf, size_t size)
{
    if (g->float_count == g->float_cap) {
        g->float_cap = g->float_cap ? g->float_cap * 2 : 16;
        g->floats = (double *)xrealloc(g->floats, (size_t)g->float_cap * sizeof(double));
    }
    g->floats[g->float_count] = v;
    snprintf(buf, size, ".LF%d", g->float_count++);
}

/* Operand of an int/bool instruction: $value or the variable's cell. */
static void int_operand(const Operand *o, char *buf, size_t size)
{
    if (o->kind == OPD_NAME)       cell_label(o->name, buf, size);
    else if (o->kind == OPD_FLOAT) snprintf(buf, size, "$%ld", (long)o->fv);
    else                           snprintf(buf, size, "$%ld", (long)o->iv);
}

/* Pushes the value of o on the x87 stack as a float. */
static void load_float(Asm *g, const Operand *o)
{
    char m[NAME_LEN + 8];
    if (o->kind == OPD_NAME) {
        cell_label(o->name, m, sizeof m);
        asm_ins(g, o->type == TAC_FLOAT ? "flds" : "fildl", m, NULL);
    } else {
        float_label(g, o->kind == OPD_FLOAT ? o->fv : (double)o->iv, m, sizeof m);
        asm_ins(g, "flds", m, NULL);
    }
}

static void emit_float_op(Asm *g, const Instr *in, const char *res)
{
    if (in->op >= OP_ADD && in->op <= OP_DIV) {
        static const char *const FLOAT_OP[] = { "fadds", "fsubs", "fmuls", "fdivs" };
        static const char *const INT_OP[]   = { "fiaddl", "fisubl", "fimull", "fidivl" };
        int k = in->op - OP_ADD;
        char m[NAME_LEN + 8];
        load_float(g, &in->a);
        if (in->b.kind == OPD_NAME) {
            cell_label(in->b.name, m, sizeof m);
            asm_ins(g, in->b.type == TAC_FLOAT ? FLOAT_OP[k] : INT_OP[k], m, NULL);
        } else {
            float_label(g, number(&in->b), m, sizeof m);
            asm_ins(g, FLOAT_OP[k], m, NULL);
        }
        asm_ins(g, "fstps", res, NULL);
        return;
    }
    /* comparison: compare a with b, the result is a 0/1 int */
    static const char *const SET[] = { "setb", "seta", "setbe", "setae", "sete", "setne" };
    load_float(g, &in->b);
    load_float(g, &in->a);
    asm_ins(g, "fcomip", "%st(1)", "%st");
    asm_ins(g, "fstp", "%st(0)", NULL);
    asm_ins(g, SET[in->op - OP_LT], "%al", NULL);
    asm_ins(g, "movzbl", "%al", "%eax");
    asm_ins(g, "movl", "%eax", res);
}

static void emit_int_op(Asm *g, const Instr *in, const char *res)
{
    char a[NAME_LEN + 8], b[NAME_LEN + 8];
    int_operand(&in->a, a, sizeof a);
    int_operand(&in->b, b, sizeof b);
    asm_ins(g, "movl", a, "%eax");
    asm_ins(g, "movl", b, "%ecx");
    switch (in->op) {
    case OP_ADD: asm_ins(g, "addl", "%ecx", "%eax");  break;
    case OP_SUB: asm_ins(g, "subl", "%ecx", "%eax");  break;
    case OP_MUL: asm_ins(g, "imull", "%ecx", "%eax"); break;
    case OP_AND: asm_ins(g, "andl", "%ecx", "%eax");  break;
    case OP_OR:  asm_ins(g, "orl", "%ecx", "%eax");   break;
    case OP_DIV:
    case OP_MOD:
        asm_ins(g, "cltd", NULL, NULL);
        asm_ins(g, "idivl", "%ecx", NULL);
        asm_ins(g, "movl", in->op == OP_DIV ? "%eax" : "%edx", res);
        return;
    default: {   /* int / bool comparison */
        static const char *const SET[] = { "setl", "setg", "setle", "setge", "sete", "setne" };
        asm_ins(g, "cmpl", "%ecx", "%eax");
        asm_ins(g, SET[in->op - OP_LT], "%al", NULL);
        asm_ins(g, "movzbl", "%al", "%eax");
    }
    }
    asm_ins(g, "movl", "%eax", res);
}

static void emit_print(Asm *g, const Instr *in)
{
    char a[NAME_LEN + 8];
    if (in->a.type == TAC_FLOAT) {               /* printf takes a double */
        asm_ins(g, "subl", "$12", "%esp");
        asm_ins(g, "movl", "$.Lfmt_float", "(%esp)");
        load_float(g, &in->a);
        asm_ins(g, "fstpl", "4(%esp)", NULL);
        asm_ins(g, "call", "_printf", NULL);
        asm_ins(g, "addl", "$12", "%esp");
        return;
    }
    int_operand(&in->a, a, sizeof a);
    if (in->a.type == TAC_BOOL) {                 /* prints true / false */
        char done[32];
        snprintf(done, sizeof done, ".Lpb%d", ++g->print_labels);
        asm_ins(g, "movl", a, "%eax");
        asm_ins(g, "cmpl", "$0", "%eax");
        asm_ins(g, "movl", "$.Lstr_true", "%ecx");
        asm_ins(g, "jne", done, NULL);
        asm_ins(g, "movl", "$.Lstr_false", "%ecx");
        text_add(&g->text, "%s:\n", done);
        asm_ins(g, "pushl", "%ecx", NULL);
        asm_ins(g, "pushl", "$.Lfmt_str", NULL);
    } else {
        asm_ins(g, "pushl", a, NULL);
        asm_ins(g, "pushl", "$.Lfmt_int", NULL);
    }
    asm_ins(g, "call", "_printf", NULL);
    asm_ins(g, "addl", "$8", "%esp");
}

/* read x: scanf("%d" or "%f", &x); a bool is read as a number, then made 0/1. */
static void emit_read(Asm *g, const Instr *in, const char *res)
{
    char addr[NAME_LEN + 16];
    g->uses_read = 1;
    snprintf(addr, sizeof addr, "$%s", res);
    asm_ins(g, "pushl", addr, NULL);
    asm_ins(g, "pushl", in->result.type == TAC_FLOAT ? "$.Lfmt_read_float" : "$.Lfmt_read_int", NULL);
    asm_ins(g, "call", "_scanf", NULL);
    asm_ins(g, "addl", "$8", "%esp");
    if (in->result.type == TAC_BOOL) {
        asm_ins(g, "movl", res, "%eax");
        asm_ins(g, "cmpl", "$0", "%eax");
        asm_ins(g, "setne", "%al", NULL);
        asm_ins(g, "movzbl", "%al", "%eax");
        asm_ins(g, "movl", "%eax", res);
    }
}

/* ----- arrays and functions (bonus) ----- */

/* %ecx = the element index of a[i]. */
static void load_index(Asm *g, const Operand *index)
{
    char m[NAME_LEN + 8];
    int_operand(index, m, sizeof m);
    asm_ins(g, "movl", m, "%ecx");
}

/* The memory operand of element %ecx of an array: _v_a(,%ecx,4). */
static void element_operand(const char *array, char *buf, size_t size)
{
    char m[NAME_LEN + 8];
    cell_label(array, m, sizeof m);
    snprintf(buf, size, "%s(,%%ecx,4)", m);
}

/* Type of parameter k of function name: the type of its k-th formal. */
static TacType formal_type(const TacProgram *p, const char *name, int k)
{
    int inside = 0, seen = 0;
    for (int i = 0; i < p->count; i++) {
        const Instr *in = &p->code[i];
        if (in->op == OP_FUNC) inside = strcmp(in->label, name) == 0;
        else if (in->op == OP_ENDFUNC) inside = 0;
        else if (inside && in->op == OP_FORMAL && seen++ == k) return in->result.type;
    }
    return TAC_INT;
}

static int formal_count(const TacProgram *p, const char *name)
{
    int inside = 0, count = 0;
    for (int i = 0; i < p->count; i++) {
        const Instr *in = &p->code[i];
        if (in->op == OP_FUNC) inside = strcmp(in->label, name) == 0;
        else if (in->op == OP_ENDFUNC) inside = 0;
        else if (inside && in->op == OP_FORMAL) count++;
    }
    return count;
}

/* param x: pushes the argument, converted to the type of the parameter it
 * belongs to (found from the call that follows the params). */
static void emit_param(Asm *g, const Instr *in)
{
    const TacProgram *p = g->program;
    int call = g->pc, before = 0;
    while (call < p->count && p->code[call].op != OP_CALL) {
        if (p->code[call].op == OP_PARAM) before++;
        call++;
    }
    TacType type = in->a.type;
    if (call < p->count) {
        int k = p->code[call].precision - before;     /* this param's position */
        if (k >= 0)
            type = formal_type(p, p->code[call].label, k);
    }
    if (type == TAC_FLOAT && !(in->a.kind == OPD_NAME && in->a.type == TAC_FLOAT)) {
        asm_ins(g, "subl", "$4", "%esp");
        load_float(g, &in->a);
        asm_ins(g, "fstps", "(%esp)", NULL);
        return;
    }
    char a[NAME_LEN + 8];
    int_operand(&in->a, a, sizeof a);       /* int, bool, or a float cell's bits */
    asm_ins(g, "pushl", a, NULL);
}

/* return x: the value goes in %eax (a float as its bits), then to the end. */
static void emit_return(Asm *g, const Instr *in)
{
    if (in->a.kind != OPD_NONE) {
        int f = cell_find(g->program, g->function);
        TacType type = f >= 0 ? g->program->cells[f].type : TAC_INT;
        if (type == TAC_FLOAT && !(in->a.kind == OPD_NAME && in->a.type == TAC_FLOAT)) {
            load_float(g, &in->a);
            asm_ins(g, "subl", "$4", "%esp");
            asm_ins(g, "fstps", "(%esp)", NULL);
            asm_ins(g, "popl", "%eax", NULL);
        } else {
            char a[NAME_LEN + 8];
            int_operand(&in->a, a, sizeof a);
            asm_ins(g, "movl", a, "%eax");
        }
    }
    char end[NAME_LEN + 16];
    snprintf(end, sizeof end, ".Lret_%s", g->function);
    asm_ins(g, "jmp", end, NULL);
}

/* func f: the callee saves its own locals (they may belong to a call of f
 * that is still running: recursion) and restores them at the end. */
static void emit_function_entry(Asm *g, const Instr *in)
{
    const TacProgram *p = g->program;
    text_add(&g->text, "\n_f_%s:\n", in->label);
    asm_ins(g, "pushl", "%ebp", NULL);
    asm_ins(g, "movl", "%esp", "%ebp");
    asm_ins(g, "pushl", "%esi", NULL);
    asm_ins(g, "pushl", "%edi", NULL);
    g->function = in->label;
    g->formal_index = 0;
    for (int k = 0; k < p->cell_count; k++) {
        const Cell *c = &p->cells[k];
        if (c->is_function || strcmp(c->owner, in->label) != 0)
            continue;
        char m[NAME_LEN + 8], n[32];
        cell_label(c->name, m, sizeof m);
        if (c->length == 0) {
            asm_ins(g, "pushl", m, NULL);
            continue;
        }
        snprintf(n, sizeof n, "$%d", 4 * c->length);   /* copy the array onto the stack */
        asm_ins(g, "subl", n, "%esp");
        snprintf(n, sizeof n, "$%d", c->length);
        char addr[NAME_LEN + 8];
        snprintf(addr, sizeof addr, "$%s", m);
        asm_ins(g, "movl", addr, "%esi");
        asm_ins(g, "movl", "%esp", "%edi");
        asm_ins(g, "movl", n, "%ecx");
        asm_ins(g, "cld", NULL, NULL);
        asm_ins(g, "rep movsl", NULL, NULL);
    }
}

/* endfunc: restores the saved locals in the opposite order and returns
 * (%eax keeps the return value). */
static void emit_function_exit(Asm *g)
{
    const TacProgram *p = g->program;
    text_add(&g->text, ".Lret_%s:\n", g->function);
    for (int k = p->cell_count - 1; k >= 0; k--) {
        const Cell *c = &p->cells[k];
        if (c->is_function || strcmp(c->owner, g->function) != 0)
            continue;
        char m[NAME_LEN + 8], n[32], addr[NAME_LEN + 8];
        cell_label(c->name, m, sizeof m);
        if (c->length == 0) {
            asm_ins(g, "popl", m, NULL);
            continue;
        }
        snprintf(addr, sizeof addr, "$%s", m);
        asm_ins(g, "movl", "%esp", "%esi");
        asm_ins(g, "movl", addr, "%edi");
        snprintf(n, sizeof n, "$%d", c->length);
        asm_ins(g, "movl", n, "%ecx");
        asm_ins(g, "cld", NULL, NULL);
        asm_ins(g, "rep movsl", NULL, NULL);
        snprintf(n, sizeof n, "$%d", 4 * c->length);
        asm_ins(g, "addl", n, "%esp");
    }
    asm_ins(g, "popl", "%edi", NULL);
    asm_ins(g, "popl", "%esi", NULL);
    asm_ins(g, "popl", "%ebp", NULL);
    asm_ins(g, "ret", NULL, NULL);
    g->function = "";
}

static void emit_instr(Asm *g, const Instr *in)
{
    char line[3 * NAME_LEN + 32], res[NAME_LEN + 8], a[NAME_LEN + 8], jump[NAME_LEN + 2];
    if (in->op == OP_OUTS) {
        text_add(&g->text, "    # outs \"%s\"\n", in->str);
        emit_out(g, in);
        return;
    }
    instr_text(in, line, sizeof line);
    text_add(&g->text, "    # %s\n", line);
    snprintf(jump, sizeof jump, ".%s", in->label);
    if (defines_result(in))
        cell_label(in->result.name, res, sizeof res);

    switch (in->op) {
    case OP_LABEL:
        text_add(&g->text, "%s:\n", jump);
        return;
    case OP_GOTO:
        asm_ins(g, "jmp", jump, NULL);
        return;
    case OP_LOAD: {                   /* res = a[i]: the element's bits */
        char m[NAME_LEN + 24];
        load_index(g, &in->b);
        element_operand(in->a.name, m, sizeof m);
        asm_ins(g, "movl", m, "%eax");
        asm_ins(g, "movl", "%eax", res);
        return;
    }
    case OP_STORE: {                  /* a[i] = x, converted to the element type */
        char m[NAME_LEN + 24];
        load_index(g, &in->a);
        element_operand(in->result.name, m, sizeof m);
        if (in->result.type == TAC_FLOAT) {
            load_float(g, &in->b);
            asm_ins(g, "fstps", m, NULL);
        } else {
            int_operand(&in->b, a, sizeof a);
            asm_ins(g, "movl", a, "%eax");
            asm_ins(g, "movl", "%eax", m);
        }
        return;
    }
    case OP_READA: {                  /* scanf into &a[i] */
        char m[NAME_LEN + 24];
        g->uses_read = 1;
        load_index(g, &in->a);
        element_operand(in->result.name, m, sizeof m);
        asm_ins(g, "leal", m, "%eax");
        asm_ins(g, "pushl", "%eax", NULL);
        asm_ins(g, "pushl", in->result.type == TAC_FLOAT ? "$.Lfmt_read_float" : "$.Lfmt_read_int", NULL);
        asm_ins(g, "call", "_scanf", NULL);
        asm_ins(g, "addl", "$8", "%esp");
        if (in->result.type == TAC_BOOL) {
            load_index(g, &in->a);
            asm_ins(g, "movl", m, "%eax");
            asm_ins(g, "cmpl", "$0", "%eax");
            asm_ins(g, "setne", "%al", NULL);
            asm_ins(g, "movzbl", "%al", "%eax");
            asm_ins(g, "movl", "%eax", m);
        }
        return;
    }
    case OP_FUNC:
        emit_function_entry(g, in);
        return;
    case OP_ENDFUNC:
        emit_function_exit(g);
        return;
    case OP_FORMAL: {                 /* argument k of n is at 8 + 4*(n-1-k)(%ebp) */
        char m[32];
        int n = formal_count(g->program, g->function);
        snprintf(m, sizeof m, "%d(%%ebp)", 8 + 4 * (n - 1 - g->formal_index++));
        asm_ins(g, "movl", m, "%eax");
        asm_ins(g, "movl", "%eax", res);
        return;
    }
    case OP_RETURN:
        emit_return(g, in);
        return;
    case OP_PARAM:
        emit_param(g, in);
        return;
    case OP_CALL: {
        char f[NAME_LEN + 8], n[32];
        snprintf(f, sizeof f, "_f_%s", in->label);
        asm_ins(g, "call", f, NULL);
        if (in->precision > 0) {
            snprintf(n, sizeof n, "$%d", 4 * in->precision);
            asm_ins(g, "addl", n, "%esp");
        }
        if (in->result.kind == OPD_NAME)
            asm_ins(g, "movl", "%eax", res);   /* the result has the function's type */
        return;
    }
    case OP_IFFALSE:
    case OP_IFTRUE:
        int_operand(&in->a, a, sizeof a);
        asm_ins(g, "movl", a, "%eax");
        asm_ins(g, "cmpl", "$0", "%eax");
        asm_ins(g, in->op == OP_IFFALSE ? "je" : "jne", jump, NULL);
        return;
    case OP_PRINT:
        emit_print(g, in);
        return;
    case OP_READ:
        emit_read(g, in, res);
        return;
    case OP_OUT:
    case OP_OUTF:
        emit_out(g, in);
        return;
    case OP_ASSIGN:
        if (in->result.type == TAC_FLOAT) {
            load_float(g, &in->a);
            asm_ins(g, "fstps", res, NULL);
        } else if (in->a.type == TAC_FLOAT) {
            load_float(g, &in->a);
            asm_ins(g, "fistpl", res, NULL);
        } else {
            int_operand(&in->a, a, sizeof a);
            asm_ins(g, "movl", a, "%eax");
            asm_ins(g, "movl", "%eax", res);
        }
        return;
    case OP_NEG:
        if (in->ty == TAC_FLOAT) {
            load_float(g, &in->a);
            asm_ins(g, "fchs", NULL, NULL);
            asm_ins(g, "fstps", res, NULL);
            return;
        }
        int_operand(&in->a, a, sizeof a);
        asm_ins(g, "movl", a, "%eax");
        asm_ins(g, "negl", "%eax", NULL);
        asm_ins(g, "movl", "%eax", res);
        return;
    case OP_NOT:
        int_operand(&in->a, a, sizeof a);
        asm_ins(g, "movl", a, "%eax");
        asm_ins(g, "xorl", "$1", "%eax");
        asm_ins(g, "movl", "%eax", res);
        return;
    default:
        if (in->ty == TAC_FLOAT) emit_float_op(g, in, res);
        else                     emit_int_op(g, in, res);
    }
}

char *tac_assembly(const TacProgram *p, const char *title, char *error, int error_size)
{
    Asm g;
    memset(&g, 0, sizeof g);
    g.program = p;
    g.function = "";
    for (int i = 0; i < p->count; i++)        /* every call must name a function of the program */
        if (p->code[i].op == OP_CALL && function_index(p, p->code[i].label) < 0) {
            if (error != NULL)
                snprintf(error, (size_t)error_size, "cannot generate assembly: unknown function '%s'",
                         p->code[i].label);
            return NULL;
        }
    text_add(&g.text, "_main:\n");
    asm_ins(&g, "pushl", "%ebp", NULL);
    asm_ins(&g, "movl", "%esp", "%ebp");
    asm_ins(&g, "andl", "$-16", "%esp");
    asm_ins(&g, "call", "___main", NULL);      /* MinGW runtime start-up */
    /* main: everything outside the func ... endfunc blocks */
    for (int i = 0, inside = 0; i < p->count; i++) {
        if (p->code[i].op == OP_FUNC) inside = 1;
        if (!inside) {
            g.pc = i;
            emit_instr(&g, &p->code[i]);
        }
        if (p->code[i].op == OP_ENDFUNC) inside = 0;
    }
    asm_ins(&g, "xorl", "%eax", "%eax");
    asm_ins(&g, "movl", "%ebp", "%esp");
    asm_ins(&g, "popl", "%ebp", NULL);
    asm_ins(&g, "ret", NULL, NULL);
    /* then each function, after main */
    for (int i = 0, inside = 0; i < p->count; i++) {
        if (p->code[i].op == OP_FUNC) inside = 1;
        if (inside) {
            g.pc = i;
            emit_instr(&g, &p->code[i]);
        }
        if (p->code[i].op == OP_ENDFUNC) inside = 0;
    }

    Text out = { NULL, 0, 0 };
    text_add(&out, "    # Generated by %s (x86, AT&T syntax, MinGW)\n", title);
    text_add(&out, "    .data\n");
    text_add(&out, ".Lfmt_int:\n    .asciz \"%%d\\n\"\n");
    text_add(&out, ".Lfmt_float:\n    .asciz \"%%g\\n\"\n");
    text_add(&out, ".Lfmt_str:\n    .asciz \"%%s\\n\"\n");
    text_add(&out, ".Lstr_true:\n    .asciz \"true\"\n");
    text_add(&out, ".Lstr_false:\n    .asciz \"false\"\n");
    if (g.uses_read) {        /* scanf formats, only for a program that reads input */
        text_add(&out, ".Lfmt_read_int:\n    .asciz \"%%d\"\n");
        text_add(&out, ".Lfmt_read_float:\n    .asciz \"%%f\"\n");
    }
    if (g.uses_printf) {      /* printf formats, only for a program with printf */
        text_add(&out, ".Lfmt_int_n:\n    .asciz \"%%d\"\n");
        text_add(&out, ".Lfmt_float_n:\n    .asciz \"%%g\"\n");
        text_add(&out, ".Lfmt_prec_n:\n    .asciz \"%%.*f\"\n");
        text_add(&out, ".Lfmt_str_n:\n    .asciz \"%%s\"\n");
    }
    for (int k = 0; k < g.string_count; k++) {
        text_add(&out, ".LS%d:\n", k);
        add_asm_string(&out, g.strings[k]);
    }
    for (int k = 0; k < g.float_count; k++) {
        char v[64];
        snprintf(v, sizeof v, "%.9g", g.floats[k]);
        text_add(&out, ".LF%d:\n    .float %s\n", k, v);
    }
    for (int k = 0; k < p->cell_count; k++) {
        char name[NAME_LEN + 8];
        if (p->cells[k].is_function)
            continue;
        cell_label(p->cells[k].name, name, sizeof name);
        if (p->cells[k].length > 0)           /* an array: length 4-byte elements */
            text_add(&out, "%s:\n    .fill %d, 4, 0\n", name, p->cells[k].length);
        else
            text_add(&out, "%s:\n    %s\n", name, p->cells[k].type == TAC_FLOAT ? ".float 0.0" : ".long 0");
    }
    text_add(&out, "    .text\n    .globl _main\n%s", text_take(&g.text));
    free(g.text.data);
    free(g.floats);
    free(g.strings);
    return text_take(&out);
}
