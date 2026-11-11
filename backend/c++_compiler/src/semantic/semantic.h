// semantic.h - Phase 3: semantic analysis (walks the AST, uses the symbol table).
#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "symbol_table.h"

// Checks the whole program; returns the number of semantic errors found.
// Fills Node::type and the unique variable names used by the TAC generator.
int analyze(BlockNode* program, SymbolTable& table);

#endif
