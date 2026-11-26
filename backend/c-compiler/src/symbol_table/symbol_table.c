/*
 * symbol_table.c - scope stack with per-scope symbol lists.
 */
#include "symbol_table.h"

#include <stdlib.h>
#include <string.h>

static void *allocate(size_t size)
{
    void *p = calloc(1, size);
    if (p == NULL) {
        perror("out of memory");
        exit(2);
    }
    return p;
}

void symtab_init(SymbolTable *table)
{
    table->current = NULL;
    table->next_scope_id = 0;
    table->first_declared = table->last_declared = NULL;
    symtab_enter_scope(table);   /* the global scope, id 0 */
}

void symtab_enter_scope(SymbolTable *table)
{
    Scope *scope = allocate(sizeof *scope);
    scope->id = table->next_scope_id++;
    scope->depth = table->current ? table->current->depth + 1 : 0;
    scope->parent = table->current;
    table->current = scope;
}

/* Pops the innermost scope. Its symbols stay in the history list (for
 * --symtab) but can no longer be found by symtab_lookup. */
void symtab_exit_scope(SymbolTable *table)
{
    Scope *scope = table->current;
    if (scope == NULL || scope->parent == NULL)
        return;                  /* never pop the global scope */
    table->current = scope->parent;
    free(scope);
}

Symbol *symtab_lookup_current(SymbolTable *table, const char *name)
{
    for (Symbol *s = table->current->symbols; s != NULL; s = s->next_in_scope)
        if (strcmp(s->name, name) == 0)
            return s;
    return NULL;
}

Symbol *symtab_lookup(SymbolTable *table, const char *name)
{
    for (Scope *scope = table->current; scope != NULL; scope = scope->parent)
        for (Symbol *s = scope->symbols; s != NULL; s = s->next_in_scope)
            if (strcmp(s->name, name) == 0)
                return s;
    return NULL;
}

Symbol *symtab_insert(SymbolTable *table, char *name, Type type, int line)
{
    if (symtab_lookup_current(table, name) != NULL)
        return NULL;

    Symbol *s = allocate(sizeof *s);
    s->name = name;
    s->tac_name = name;
    for (Symbol *old = table->first_declared; old != NULL; old = old->next_declared) {
        if (strcmp(old->name, name) == 0) {
            s->tac_name = allocate(strlen(name) + 16);
            sprintf(s->tac_name, "%s@%d", name, table->current->id);
            break;
        }
    }
    s->type = type;
    s->scope_id = table->current->id;
    s->depth = table->current->depth;
    s->line = line;

    s->next_in_scope = table->current->symbols;
    table->current->symbols = s;

    if (table->last_declared)
        table->last_declared->next_declared = s;
    else
        table->first_declared = s;
    table->last_declared = s;
    return s;
}

/* The type as shown by --symtab: int, an array int[5], or a function int(int,float). */
static void type_text(const Symbol *s, char *type, size_t size)
{
    snprintf(type, size, "%s", type_name(s->type));
    if (s->length > 0)
        snprintf(type + strlen(type), size - strlen(type), "[%ld]", s->length);
    if (s->is_function) {
        snprintf(type + strlen(type), size - strlen(type), "(");
        for (size_t i = 0; s->params != NULL && i < s->params->count; i++)
            snprintf(type + strlen(type), size - strlen(type), "%s%s", i ? "," : "",
                     type_name(s->params->items[i].type));
        snprintf(type + strlen(type), size - strlen(type), ")");
    }
}

void symtab_print(SymbolTable *table, FILE *out)
{
    char type[96];
    int width = 6;                       /* the Type column fits the longest type */
    for (Symbol *s = table->first_declared; s != NULL; s = s->next_declared) {
        type_text(s, type, sizeof type);
        if ((int)strlen(type) > width)
            width = (int)strlen(type);
    }
    fprintf(out, "%-16s %-*s %-6s %-6s %-6s %s\n", "Name", width, "Type", "Scope", "Depth", "Line", "TAC name");
    fprintf(out, "%-16s %-*s %-6s %-6s %-6s %s\n", "----", width, "----", "-----", "-----", "----", "--------");
    for (Symbol *s = table->first_declared; s != NULL; s = s->next_declared) {
        type_text(s, type, sizeof type);
        fprintf(out, "%-16s %-*s %-6d %-6d %-6d %s\n",
                s->name, width, type, s->scope_id, s->depth, s->line, s->tac_name);
    }
}
