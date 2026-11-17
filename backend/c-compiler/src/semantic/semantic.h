/*
 * semantic.h - semantic analysis (scope and type checking) of the AST.
 */
#ifndef TRICORA_SEMANTIC_H
#define TRICORA_SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"

/* Walks the whole program, records declarations in `table`, annotates every
 * expression with its type and prints one message per error to stderr.
 * Returns the number of semantic errors found (0 = program is valid). */
int semantic_check(StmtList *program, SymbolTable *table);

#endif
