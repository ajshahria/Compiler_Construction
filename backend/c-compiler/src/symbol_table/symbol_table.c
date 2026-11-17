#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>
void symbols_init(SymbolTable*t){t->head=NULL;t->depth=0;}
void symbols_enter(SymbolTable*t){t->depth++;}
void symbols_leave(SymbolTable*t){Symbol**p=&t->head;while(*p){if((*p)->depth==t->depth){Symbol*x=*p;*p=x->next;free(x);}else p=&(*p)->next;}if(t->depth>0)t->depth--;}
Symbol *symbols_lookup(SymbolTable*t,const char*n){for(Symbol*s=t->head;s;s=s->next)if(!strcmp(s->name,n))return s;return NULL;}
int symbols_insert(SymbolTable*t,char*n,Type ty){for(Symbol*s=t->head;s;s=s->next)if(s->depth==t->depth&&!strcmp(s->name,n))return 0;Symbol*s=malloc(sizeof(*s));if(!s)return 0;s->name=n;s->type=ty;s->depth=t->depth;s->next=t->head;t->head=s;return 1;}

