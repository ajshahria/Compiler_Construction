/*
 * backend.h - back end of the compiler: works on the Three Address Code
 * (TAC) text written by the front end (codegen).
 *
 *   tac_build      reads the TAC text into a list of instructions
 *   tac_optimize   --opt  constant propagation and folding, unreachable code,
 *                         dead temporaries, jump clean-up
 *   tac_run        --run  executes the TAC (interpreter) and returns its output
 *   tac_assembly   --asm  32-bit x86 assembly (AT&T syntax, MinGW gcc)
 *
 * The TAC instruction forms are the ones the front end writes:
 *     x = y            x = - y         x = ! y         x = y op z
 *     L1:              goto L1         ifFalse x goto L1    if x goto L1
 *     print x
 * TAC text carries no types, so tac_build gets the declared type of every
 * variable from the symbol table; the type of each temporary is worked out
 * from the instruction that defines it.
 *
 * Written in plain C that also compiles as C++, so the same file is used by
 * the C compiler (backend.c) and the Java compiler (backend.cpp).
 */
#ifndef TAC_BACKEND_H
#define TAC_BACKEND_H

typedef enum { TAC_INT, TAC_FLOAT, TAC_BOOL } TacType;

/* One declared variable: its TAC name (x, or x@3 for a shadowing one) and type. */
typedef struct {
    const char *name;
    TacType type;
} TacVar;

typedef struct TacProgram TacProgram;

/* Reads TAC text. Returns NULL and writes a message into `error` (if not NULL)
 * when a line cannot be read. */
TacProgram *tac_build(const char *text, const TacVar *vars, int var_count,
                      char *error, int error_size);
void tac_free(TacProgram *program);

void tac_optimize(TacProgram *program);

/* The program as TAC text again (used to print the optimized TAC).
 * The caller frees the result. */
char *tac_text(const TacProgram *program);

/* Runs the program. Returns its output (caller frees). *runtime_error is set
 * to a message (static text) when it stops with an error, or NULL. */
char *tac_run(const TacProgram *program, const char **runtime_error);

/* 32-bit x86 assembly for MinGW gcc (caller frees). `title` names the
 * compiler in the first comment line. */
char *tac_assembly(const TacProgram *program, const char *title);

#endif
