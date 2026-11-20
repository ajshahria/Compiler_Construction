/*
 * codegen.h - Three Address Code (TAC) generation from the checked AST.
 */
#ifndef TRICORA_CODEGEN_H
#define TRICORA_CODEGEN_H

#include "ast.h"

#include <stdio.h>

/* Writes the TAC of the whole program to `out`. Must only be called on a
 * program that passed semantic analysis. */
void generate_tac(StmtList *program, FILE *out);

/* The same TAC as text, for the back end (caller frees). */
char *generate_tac_text(StmtList *program);

#endif
