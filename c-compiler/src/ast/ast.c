#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

static void *alloc(size_t n) { void *p=calloc(1,n); if(!p){perror("out of memory");exit(2);} return p; }
const char *type_name(Type t){switch(t){case TYPE_INT:return "int";case TYPE_FLOAT:return "float";case TYPE_BOOL:return "bool";default:return "error";}}
Expr *expr_int(long v,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_INT;e->line=l;e->as.integer=v;return e;}
Expr *expr_float(double v,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_FLOAT;e->line=l;e->as.real=v;return e;}
Expr *expr_bool(int v,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_BOOL;e->line=l;e->as.boolean=v;return e;}
Expr *expr_name(char*n,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_NAME;e->line=l;e->as.name=n;return e;}
Expr *expr_binary(int o,Expr*a,Expr*b,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_BINARY;e->line=l;e->as.binary.op=o;e->as.binary.left=a;e->as.binary.right=b;return e;}
Expr *expr_unary(int o,Expr*a,int l){Expr*e=alloc(sizeof(*e));e->kind=EX_UNARY;e->line=l;e->as.unary.op=o;e->as.unary.child=a;return e;}
StmtList *list_new(void){return alloc(sizeof(StmtList));}
StmtList *list_add(StmtList*l,Stmt*s){if(!l)return list_new();if(l->count==l->capacity){l->capacity=l->capacity?l->capacity*2:8;l->items=realloc(l->items,l->capacity*sizeof(*l->items));if(!l->items){perror("out of memory");exit(2);}}l->items[l->count++]=s;return l;}
static Stmt *news(StmtKind k,int l){Stmt*s=alloc(sizeof(*s));s->kind=k;s->line=l;return s;}
Stmt *stmt_decl(Type t,char*n,Expr*e,int l){Stmt*s=news(ST_DECL,l);s->as.decl.type=t;s->as.decl.name=n;s->as.decl.init=e;return s;}
Stmt *stmt_assign(char*n,Expr*e,int l){Stmt*s=news(ST_ASSIGN,l);s->as.assign.name=n;s->as.assign.value=e;return s;}
Stmt *stmt_print(Expr*e,int l){Stmt*s=news(ST_PRINT,l);s->as.print=e;return s;}
Stmt *stmt_if(Expr*c,Stmt*a,Stmt*b,int l){Stmt*s=news(ST_IF,l);s->as.branch.condition=c;s->as.branch.yes=a;s->as.branch.no=b;return s;}
Stmt *stmt_while(Expr*c,Stmt*b,int l){Stmt*s=news(ST_WHILE,l);s->as.loop.condition=c;s->as.loop.body=b;return s;}
Stmt *stmt_block(StmtList*l,int n){Stmt*s=news(ST_BLOCK,n);s->as.block=l;return s;}
Stmt *stmt_error(int l){return news(ST_ERROR,l);}
static void tabs(int n){while(n--)putchar(' ');}
static const char *opname(int o){switch(o){case OP_ADD:return "+";case OP_SUB:return "-";case OP_MUL:return "*";case OP_DIV:return "/";case OP_MOD:return "%";case OP_EQ:return "==";case OP_NE:return "!=";case OP_LT:return "<";case OP_LE:return "<=";case OP_GT:return ">";case OP_GE:return ">=";case OP_AND:return "&&";case OP_OR:return "||";case OP_NOT:return "!";default:return "-";}}
static void pe(Expr*e){if(!e){printf("<missing>");return;}switch(e->kind){case EX_INT:printf("%ld",e->as.integer);break;case EX_FLOAT:printf("%g",e->as.real);break;case EX_BOOL:printf("%s",e->as.boolean?"true":"false");break;case EX_NAME:printf("%s",e->as.name);break;case EX_BINARY:printf("(");pe(e->as.binary.left);printf(" %s ",opname(e->as.binary.op));pe(e->as.binary.right);printf(")");break;case EX_UNARY:printf("%s",opname(e->as.unary.op));pe(e->as.unary.child);break;}}
static void ps(Stmt*s,int d){if(!s)return;tabs(d);switch(s->kind){case ST_BLOCK:puts("Block");for(size_t i=0;s->as.block&&i<s->as.block->count;i++)ps(s->as.block->items[i],d+2);break;case ST_DECL:printf("Declare %s %s",type_name(s->as.decl.type),s->as.decl.name);if(s->as.decl.init){printf(" = ");pe(s->as.decl.init);}puts("");break;case ST_ASSIGN:printf("Assign %s = ",s->as.assign.name);pe(s->as.assign.value);puts("");break;case ST_PRINT:printf("Print ");pe(s->as.print);puts("");break;case ST_EXPR:printf("Expression ");pe(s->as.expression);puts("");break;case ST_IF:printf("If ");pe(s->as.branch.condition);puts("");ps(s->as.branch.yes,d+2);if(s->as.branch.no){tabs(d);puts("Else");ps(s->as.branch.no,d+2);}break;case ST_WHILE:printf("While ");pe(s->as.loop.condition);puts("");ps(s->as.loop.body,d+2);break;case ST_ERROR:puts("Recovered syntax error");break;}}
void ast_print(StmtList*l){for(size_t i=0;l&&i<l->count;i++)ps(l->items[i],0);}

