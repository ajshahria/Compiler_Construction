#ifndef TRICORA_SYMBOL_TABLE_H
#define TRICORA_SYMBOL_TABLE_H
#include "ast.h"
typedef struct Symbol { char *name; Type type; int depth; struct Symbol *next; } Symbol;
typedef struct { Symbol *head; int depth; } SymbolTable;
void symbols_init(SymbolTable *t); void symbols_enter(SymbolTable *t); void symbols_leave(SymbolTable *t);
Symbol *symbols_lookup(SymbolTable *t,const char *name); int symbols_insert(SymbolTable *t,char *name,Type type);
#endif

