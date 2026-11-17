/*
 * main.c - command-line driver of the Tricora C compiler.
 *
 * Runs the pipeline   source -> lexer -> parser/AST -> semantic analysis
 * (symbol table + type checking) -> three address code   and prints the
 * output of the phase chosen on the command line.
 *
 * Exit status: 0 = success, 1 = errors in the program, 2 = usage/file problem.
 */
#include "ast.h"
#include "codegen.h"
#include "semantic.h"
#include "symbol_table.h"
#include "parser.tab.h"

#include <stdio.h>
#include <string.h>

extern FILE *yyin;
extern char *yytext;
extern int yylineno;
extern int yylex(void);
extern void yyrestart(FILE *input);
extern int yyparse(void);

extern StmtList *program_root;   /* set by the parser   */
extern int lexer_errors;         /* counted by the lexer  */
extern int parser_errors;        /* counted by the parser */

typedef enum { MODE_TOKENS, MODE_AST, MODE_SYMTAB, MODE_TAC, MODE_ALL } Mode;

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s [option] <source-file> [-o output-file]\n"
        "Options:\n"
        "  --tokens   print the token stream (lexical analysis)\n"
        "  --ast      print the abstract syntax tree\n"
        "  --symtab   print the symbol table after semantic analysis\n"
        "  --tac      print three address code (default)\n"
        "  --all      print tokens, AST, symbol table and TAC\n",
        program);
}

static const char *token_name(int token)
{
    static char single[8];
    switch (token) {
    case IDENT:         return "IDENTIFIER";
    case INT_LITERAL:   return "INT_LITERAL";
    case FLOAT_LITERAL: return "FLOAT_LITERAL";
    case INT:   return "KEYWORD_INT";
    case FLOAT: return "KEYWORD_FLOAT";
    case BOOL:  return "KEYWORD_BOOL";
    case IF:    return "KEYWORD_IF";
    case ELSE:  return "KEYWORD_ELSE";
    case WHILE: return "KEYWORD_WHILE";
    case PRINT: return "KEYWORD_PRINT";
    case TRUE:  return "BOOL_LITERAL";
    case FALSE: return "BOOL_LITERAL";
    case EQ: case NE: case LE: case GE:
        return "RELATIONAL_OP";
    case AND: case OR:
        return "LOGICAL_OP";
    }
    switch (token) {
    case '+': case '-': case '*': case '/': case '%': return "ARITHMETIC_OP";
    case '<': case '>':                               return "RELATIONAL_OP";
    case '!':                                         return "LOGICAL_OP";
    case '=':                                         return "ASSIGN";
    case '(': case ')': case '{': case '}': case ';': return "DELIMITER";
    }
    snprintf(single, sizeof single, "'%c'", token);
    return single;
}

/* Phase 1 on its own: print every token with its line. */
static void print_tokens(FILE *out)
{
    fprintf(out, "%-6s %-16s %s\n", "Line", "Token", "Lexeme");
    int token;
    while ((token = yylex()) != 0)
        fprintf(out, "%-6d %-16s %s\n", yylloc.first_line, token_name(token), yytext);
}

static void heading(FILE *out, const char *title)
{
    fprintf(out, "===== %s =====\n", title);
}

int main(int argc, char **argv)
{
    Mode mode = MODE_TAC;
    const char *input = NULL;
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0)      mode = MODE_TOKENS;
        else if (strcmp(argv[i], "--ast") == 0)    mode = MODE_AST;
        else if (strcmp(argv[i], "--symtab") == 0) mode = MODE_SYMTAB;
        else if (strcmp(argv[i], "--tac") == 0 || strcmp(argv[i], "--ir") == 0) mode = MODE_TAC;
        else if (strcmp(argv[i], "--all") == 0)    mode = MODE_ALL;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output = argv[++i];
        else if (argv[i][0] != '-' && input == NULL) input = argv[i];
        else {
            usage(argv[0]);
            return 2;
        }
    }
    if (input == NULL) {
        usage(argv[0]);
        return 2;
    }

    FILE *out = stdout;
    if (output != NULL && (out = fopen(output, "w")) == NULL) {
        perror(output);
        return 2;
    }

    /* ---- Phase 1: lexical analysis (token listing only) ---- */
    if (mode == MODE_TOKENS || mode == MODE_ALL) {
        if ((yyin = fopen(input, "r")) == NULL) {
            perror(input);
            return 2;
        }
        if (mode == MODE_ALL)
            heading(out, "TOKENS");
        print_tokens(out);
        fclose(yyin);
        if (lexer_errors) {
            fflush(out);
            fprintf(stderr, "Compilation failed: %d lexical error(s).\n", lexer_errors);
            return 1;
        }
        if (mode == MODE_TOKENS)
            return 0;
        yylineno = 1;   /* --all: scan the file again for the parser */
    }

    /* ---- Phase 2: syntax analysis, builds the AST ---- */
    if ((yyin = fopen(input, "r")) == NULL) {
        perror(input);
        return 2;
    }
    yyrestart(yyin);
    yyparse();
    fclose(yyin);

    if (lexer_errors || parser_errors || program_root == NULL) {
        fflush(out);
        fprintf(stderr, "Compilation failed: %d lexical error(s), %d syntax error(s).\n",
                lexer_errors, parser_errors);
        return 1;
    }

    if (mode == MODE_AST || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "ABSTRACT SYNTAX TREE");
        ast_print_program(program_root, out);
        if (mode == MODE_AST)
            return 0;
    }

    /* ---- Phase 3: semantic analysis with the symbol table ---- */
    SymbolTable table;
    int semantic_errors = semantic_check(program_root, &table);
    if (semantic_errors) {
        fflush(out);
        fprintf(stderr, "Compilation failed: %d semantic error(s).\n", semantic_errors);
        return 1;
    }

    if (mode == MODE_SYMTAB || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "SYMBOL TABLE");
        symtab_print(&table, out);
        if (mode == MODE_SYMTAB)
            return 0;
    }

    /* ---- Phase 4: intermediate code generation ---- */
    if (mode == MODE_ALL)
        heading(out, "THREE ADDRESS CODE");
    generate_tac(program_root, out);

    if (out != stdout)
        fclose(out);
    return 0;
}
