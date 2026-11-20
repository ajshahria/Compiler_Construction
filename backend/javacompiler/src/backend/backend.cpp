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
    OP_LABEL, OP_GOTO, OP_IFFALSE, OP_IFTRUE, OP_PRINT
} OpCode;

/* Operator text for OP_ADD .. OP_NOT, indexed by OpCode. */
static const char *const OP_SYMBOL[] = {
    "", "+", "-", "*", "/", "%", "<", ">", "<=", ">=", "==", "!=", "&&", "||", "-", "!"
};

typedef struct {
    OpCode op;
    Operand result, a, b;
    TacType ty;            /* type the operation works in */
    char label[NAME_LEN];  /* OP_LABEL / OP_GOTO / OP_IF* target */
} Instr;

/* Every variable and temporary: its name and type. A declared variable keeps
 * the type from the symbol table; a temporary takes the type of its value. */
typedef struct {
    char name[NAME_LEN];
    TacType type;
    int declared;
} Cell;

struct TacProgram {
    Instr *code;
    int count, cap;
    Cell *cells;
    int cell_count, cell_cap;
};

static int is_binary(OpCode op)  { return op >= OP_ADD && op <= OP_OR; }
static int is_compare(OpCode op) { return op >= OP_LT && op <= OP_NE; }
static int defines_result(OpCode op)
{
    return op == OP_ASSIGN || op == OP_NEG || op == OP_NOT || is_binary(op);
}
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
    default:         snprintf(buf, size, "%s = %s %s %s", r, a, OP_SYMBOL[in->op], b); break;
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
    for (char *t = strtok(line, " \t\r"); t != NULL && n < 8; t = strtok(NULL, " \t\r"))
        tok[n++] = t;
    memset(in, 0, sizeof *in);
    in->result.slot = in->a.slot = in->b.slot = -1;

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
        } else {
            in->ty = a;       /* assign, negate, print, if */
        }

        if (defines_result(in->op)) {
            int k = cell_find(p, in->result.name);
            if (k < 0)
                k = cell_add(p, in->result.name, value, 0);
            else if (!p->cells[k].declared)
                p->cells[k].type = value;
            in->result.slot = k;
            in->result.type = p->cells[k].type;
            if (in->op == OP_ASSIGN)
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
        if (cell_find(p, vars[k].name) < 0)
            cell_add(p, vars[k].name, vars[k].type, 1);

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
    free(p->code);
    free(p->cells);
    free(p);
}

char *tac_text(const TacProgram *p)
{
    Text t = { NULL, 0, 0 };
    char line[3 * NAME_LEN + 32];
    for (int i = 0; i < p->count; i++) {
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
        if (in.op == OP_LABEL) {
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
        if (defines_result(in.op)) {
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
        p->code[n++] = in;
    }
    p->count = n;
    free(value);
    free(known);
}

/* Pass 2: code after an unconditional jump is unreachable until the next label. */
static void unreachable_pass(TacProgram *p)
{
    int n = 0, dead = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->code[i].op == OP_LABEL) dead = 0;
        if (dead) continue;
        p->code[n++] = p->code[i];
        if (p->code[i].op == OP_GOTO) dead = 1;
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
        if (defines_result(in->op)) defs[in->result.slot]++;
    }
    int n = 0;
    for (int i = 0; i < p->count; i++) {
        Instr in = p->code[i];
        int temp = defines_result(in.op) && !p->cells[in.result.slot].declared;
        if (temp && uses[in.result.slot] == 0)
            continue;
        if (in.op == OP_ASSIGN && in.a.kind == OPD_NAME && !p->cells[in.a.slot].declared
            && uses[in.a.slot] == 1 && defs[in.a.slot] == 1 && n > 0) {
            Instr *prev = &p->code[n - 1];
            if (defines_result(prev->op) && prev->result.slot == in.a.slot
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

static int label_index(const TacProgram *p, const char *label)
{
    for (int i = 0; i < p->count; i++)
        if (p->code[i].op == OP_LABEL && strcmp(p->code[i].label, label) == 0)
            return i;
    return -1;
}

char *tac_run(const TacProgram *p, const char **runtime_error)
{
    Text out = { NULL, 0, 0 };
    *runtime_error = NULL;
    int cells = p->cell_count ? p->cell_count : 1;
    Value *mem = (Value *)xalloc((size_t)cells * sizeof(Value));
    for (int k = 0; k < p->cell_count; k++) {     /* every variable starts at 0 */
        mem[k].ty = p->cells[k].type; mem[k].iv = 0; mem[k].fv = 0;
    }
    int *target = (int *)xalloc((size_t)(p->count ? p->count : 1) * sizeof(int));
    for (int i = 0; i < p->count; i++)
        target[i] = p->code[i].op == OP_GOTO || p->code[i].op == OP_IFFALSE || p->code[i].op == OP_IFTRUE
                  ? label_index(p, p->code[i].label) : -1;

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
} Asm;

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

static void emit_instr(Asm *g, const Instr *in)
{
    char line[3 * NAME_LEN + 32], res[NAME_LEN + 8], a[NAME_LEN + 8], jump[NAME_LEN + 2];
    instr_text(in, line, sizeof line);
    text_add(&g->text, "    # %s\n", line);
    snprintf(jump, sizeof jump, ".%s", in->label);
    if (defines_result(in->op))
        cell_label(in->result.name, res, sizeof res);

    switch (in->op) {
    case OP_LABEL:
        text_add(&g->text, "%s:\n", jump);
        return;
    case OP_GOTO:
        asm_ins(g, "jmp", jump, NULL);
        return;
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

char *tac_assembly(const TacProgram *p, const char *title)
{
    Asm g;
    memset(&g, 0, sizeof g);
    text_add(&g.text, "_main:\n");
    asm_ins(&g, "pushl", "%ebp", NULL);
    asm_ins(&g, "movl", "%esp", "%ebp");
    asm_ins(&g, "andl", "$-16", "%esp");
    asm_ins(&g, "call", "___main", NULL);      /* MinGW runtime start-up */
    for (int i = 0; i < p->count; i++)
        emit_instr(&g, &p->code[i]);
    asm_ins(&g, "xorl", "%eax", "%eax");
    asm_ins(&g, "movl", "%ebp", "%esp");
    asm_ins(&g, "popl", "%ebp", NULL);
    asm_ins(&g, "ret", NULL, NULL);

    Text out = { NULL, 0, 0 };
    text_add(&out, "    # Generated by %s (x86, AT&T syntax, MinGW)\n", title);
    text_add(&out, "    .data\n");
    text_add(&out, ".Lfmt_int:\n    .asciz \"%%d\\n\"\n");
    text_add(&out, ".Lfmt_float:\n    .asciz \"%%g\\n\"\n");
    text_add(&out, ".Lfmt_str:\n    .asciz \"%%s\\n\"\n");
    text_add(&out, ".Lstr_true:\n    .asciz \"true\"\n");
    text_add(&out, ".Lstr_false:\n    .asciz \"false\"\n");
    for (int k = 0; k < g.float_count; k++) {
        char v[64];
        snprintf(v, sizeof v, "%.9g", g.floats[k]);
        text_add(&out, ".LF%d:\n    .float %s\n", k, v);
    }
    for (int k = 0; k < p->cell_count; k++) {
        char name[NAME_LEN + 8];
        cell_label(p->cells[k].name, name, sizeof name);
        text_add(&out, "%s:\n    %s\n", name, p->cells[k].type == TAC_FLOAT ? ".float 0.0" : ".long 0");
    }
    text_add(&out, "    .text\n    .globl _main\n%s", text_take(&g.text));
    free(g.text.data);
    free(g.floats);
    return text_take(&out);
}
