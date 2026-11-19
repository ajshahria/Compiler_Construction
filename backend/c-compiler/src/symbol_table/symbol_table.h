/*
 * symbol_table.h - scoped symbol table.
 *
 * Scopes form a stack: the global scope is at the bottom and every block
 * ({ ... }, including if/while bodies) pushes a new one. A lookup searches from
 * the innermost scope outwards, so inner declarations shadow outer ones, and a
 * scope's symbols stop being visible as soon as the block is left.
 *
 * Every symbol ever declared is also kept in a history list so the whole table
 * can be printed after analysis (the --symtab option).
 */
#ifndef TRICORA_SYMBOL_TABLE_H
#define TRICORA_SYMBOL_TABLE_H

#include "ast.h"

#include <stdio.h>

typedef struct Symbol {
    char *name;
    char *tac_name;        /* name used in the TAC: `x`, or `x@3` when another
                              variable called x was declared earlier (shadowing),
                              so the two never share storage. '@' cannot appear
                              in source identifiers, so this cannot clash. */
    Type type;
    int scope_id;          /* unique number of the declaring scope (0 = global) */
    int depth;             /* nesting depth of that scope (0 = global)          */
    int line;              /* line where it was declared                        */
    struct Symbol *next_in_scope;   /* next symbol in the same scope            */
    struct Symbol *next_declared;   /* history list in declaration order        */
} Symbol;

typedef struct Scope {
    int id;
    int depth;
    Symbol *symbols;
    struct Scope *parent;
} Scope;

typedef struct {
    Scope *current;        /* innermost open scope */
    int next_scope_id;
    Symbol *first_declared, *last_declared;
} SymbolTable;

void symtab_init(SymbolTable *table);
void symtab_enter_scope(SymbolTable *table);
void symtab_exit_scope(SymbolTable *table);

/* Adds a symbol to the current scope. Returns NULL if the name is already
 * declared in this same scope (a redeclaration). */
Symbol *symtab_insert(SymbolTable *table, char *name, Type type, int line);

/* Finds the visible declaration of name, searching inner to outer scopes. */
Symbol *symtab_lookup(SymbolTable *table, const char *name);

/* Finds name only in the current scope. */
Symbol *symtab_lookup_current(SymbolTable *table, const char *name);

/* Prints every declared symbol: name, type, scope, line. */
void symtab_print(SymbolTable *table, FILE *out);

#endif
