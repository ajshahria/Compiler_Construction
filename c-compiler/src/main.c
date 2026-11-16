#include "ast.h"
#include "codegen.h"
#include "semantic.h"
#include "parser.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int yyparse(void); extern FILE *yyin; StmtList *program_root;
extern int lexer_errors, parser_errors;
const char *token_name(int t){switch(t){case IDENT:return "IDENTIFIER";case INT_LITERAL:return "INT_LITERAL";case FLOAT_LITERAL:return "FLOAT_LITERAL";case INT:return "int";case FLOAT:return "float";case BOOL:return "bool";case IF:return "if";case ELSE:return "else";case WHILE:return "while";case PRINT:return "print";case TRUE:return "true";case FALSE:return "false";case EQ:return "==";case NE:return "!=";case LE:return "<=";case GE:return ">=";case AND:return "&&";case OR:return "||";default:break;}static char b[16];if(t>0&&t<128){snprintf(b,sizeof b,"'%c'",t);return b;}return "TOKEN";}
static void usage(const char *p){fprintf(stderr,"Usage: %s [--tokens|--ast|--ir] <source.tri> [-o output]\n",p);}
int main(int argc,char **argv){const char*mode="--ir",*input=NULL,*output=NULL;for(int i=1;i<argc;i++){if(!strcmp(argv[i],"--tokens")||!strcmp(argv[i],"--ast")||!strcmp(argv[i],"--ir"))mode=argv[i];else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];else if(!input)input=argv[i];else{usage(argv[0]);return 2;}}if(!input){usage(argv[0]);return 2;}yyin=fopen(input,"r");if(!yyin){perror(input);return 2;}if(!strcmp(mode,"--tokens")){extern int yylex(void);extern char *yytext;extern int yylineno;extern const char *token_name(int);int t;while((t=yylex()))printf("line %-4d %-14s %s\n",yylineno,token_name(t),yytext);fclose(yyin);return lexer_errors?1:0;}int parse=yyparse();fclose(yyin);if(parse||!program_root||lexer_errors||parser_errors){fprintf(stderr,"Compilation stopped: lexical or syntax errors found.\n");return 1;}if(semantic_check(program_root)){fprintf(stderr,"Compilation stopped: semantic errors found.\n");return 1;}if(!strcmp(mode,"--ast")){ast_print(program_root);return 0;}FILE*out=output?fopen(output,"w"):stdout;if(!out){perror(output);return 2;}generate_ir(program_root,out);if(output)fclose(out);return 0;}

