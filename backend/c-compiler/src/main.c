/*
 * main.c - command-line driver of the Tricora C compiler.
 *
 * Runs the pipeline   source -> lexer -> parser/AST -> semantic analysis
 * (symbol table + type checking) -> three address code   and prints the
 * output of the phase chosen on the command line. The back end (backend.c)
 * then optimizes the TAC, translates it to x86 assembly or runs it.
 *
 * Exit status: 0 = success, 1 = errors in the program (or a runtime error
 * with --run), 2 = usage/file problem.
 */
#include "ast.h"
#include "backend.h"
#include "codegen.h"
#include "semantic.h"
#include "symbol_table.h"
#include "parser.tab.h"

#include <stdio.h>
#include <stdlib.h>
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
extern void print_headers(FILE *out);   /* lexer.l: table of the #include headers */

typedef enum { MODE_HEADERS, MODE_TOKENS, MODE_AST, MODE_DOT, MODE_SYMTAB, MODE_TAC, MODE_OPT, MODE_ASM, MODE_RUN, MODE_ALL } Mode;

static void usage(const char *program)
{
    fprintf(stderr,
        "Usage: %s [option] <source-file> [-o output-file]\n"
        "Options:\n"
        "  --headers  list the #include headers found (preprocessor step)\n"
        "  --tokens  print the token stream (lexical analysis)\n"
        "  --ast      print the abstract syntax tree\n"
        "  --dot      print the abstract syntax tree as a Graphviz graph\n"
        "             (render it with: dot -Tpng ast.dot -o ast.png)\n"
        "  --symtab   print the symbol table after semantic analysis\n"
        "  --tac      print three address code (default)\n"
        "  --opt      print optimized three address code\n"
        "  --asm      print x86 assembly (32-bit, AT&T syntax, MinGW)\n"
        "  --run      run the program (TAC interpreter) and print its output\n"
        "  --all      print every phase: tokens, headers, AST, symbol table,\n"
        "             TAC, optimized TAC, assembly and the program output\n",
        program);
}

static const char *token_name(int token)
{
    static char single[8];
    switch (token) {
    case IDENT:         return "IDENTIFIER";
    case STRING_LITERAL: return "STRING_LITERAL";
    case RETURN: return "KEYWORD_RETURN";
    case VOID:   return "KEYWORD_VOID";
    case PRINTF: return "KEYWORD_PRINTF";
    case SCANF:  return "KEYWORD_SCANF";
    case INT_LITERAL:   return "INT_LITERAL";
    case FLOAT_LITERAL: return "FLOAT_LITERAL";
    case INT:   return "KEYWORD_INT";
    case FLOAT: return "KEYWORD_FLOAT";
    case BOOL:  return "KEYWORD_BOOL";
    case IF:    return "KEYWORD_IF";
    case ELSE:  return "KEYWORD_ELSE";
    case WHILE: return "KEYWORD_WHILE";
    case FOR:      return "KEYWORD_FOR";
    case DO:       return "KEYWORD_DO";
    case SWITCH:   return "KEYWORD_SWITCH";
    case CASE:     return "KEYWORD_CASE";
    case DEFAULT:  return "KEYWORD_DEFAULT";
    case BREAK:    return "KEYWORD_BREAK";
    case CONTINUE: return "KEYWORD_CONTINUE";
    case INC: case DEC:
        return "INCDEC_OP";
    case ADD_ASSIGN: case SUB_ASSIGN: case MUL_ASSIGN: case DIV_ASSIGN: case MOD_ASSIGN:
        return "COMPOUND_ASSIGN";
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
    case '&':                                         return "ADDRESS_OP";
    case '(': case ')': case '{': case '}': case ';': case ',':
    case '[': case ']': case ':':                     return "DELIMITER";
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

/* Back end: reads the TAC with the declared variable types from the symbol
 * table, optimizes it, and prints the optimized TAC, the assembly and/or the
 * program output. --asm and --run use the optimized TAC. Returns the exit
 * status: 1 if the program stopped with a runtime error. */
static int back_end(const char *tac, SymbolTable *table, Mode mode, FILE *out)
{
    int count = 0;
    for (Symbol *s = table->first_declared; s != NULL; s = s->next_declared)
        count++;
    TacVar *vars = malloc((size_t)(count ? count : 1) * sizeof *vars);
    if (vars == NULL) {
        perror("out of memory");
        return 2;
    }
    int k = 0;
    for (Symbol *s = table->first_declared; s != NULL; s = s->next_declared, k++) {
        vars[k].name = s->tac_name;
        vars[k].type = s->type == TYPE_FLOAT ? TAC_FLOAT : s->type == TYPE_BOOL ? TAC_BOOL : TAC_INT;
        vars[k].length = (int)s->length;
        vars[k].is_function = s->is_function;
        vars[k].function = s->function;
    }
    char error[256];
    TacProgram *program = tac_build(tac, vars, count, error, sizeof error);
    free(vars);
    if (program == NULL) {
        fprintf(stderr, "internal error: %s\n", error);
        return 2;
    }
    tac_optimize(program);

    int status = 0;
    if (mode == MODE_OPT || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "OPTIMIZED THREE ADDRESS CODE");
        char *text = tac_text(program);
        fputs(text, out);
        free(text);
    }
    if (mode == MODE_ASM || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "X86 ASSEMBLY");
        char *text = tac_assembly(program, "the Tricora C compiler", error, sizeof error);
        if (text == NULL) {
            fflush(out);
            fprintf(stderr, "%s\n", error);
            tac_free(program);
            return 1;
        }
        fputs(text, out);
        free(text);
    }
    if (mode == MODE_RUN || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "PROGRAM OUTPUT");
        const char *runtime_error;
        char *text = tac_run(program, &runtime_error);
        fputs(text, out);
        free(text);
        if (runtime_error != NULL) {
            fflush(out);
            fprintf(stderr, "%s\n", runtime_error);
            status = 1;
        }
    }
    tac_free(program);
    return status;
}

int main(int argc, char **argv)
{
    Mode mode = MODE_TAC;
    const char *input = NULL;
    const char *output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--headers") == 0)     mode = MODE_HEADERS;
        else if (strcmp(argv[i], "--tokens") == 0) mode = MODE_TOKENS;
        else if (strcmp(argv[i], "--ast") == 0)    mode = MODE_AST;
        else if (strcmp(argv[i], "--dot") == 0)    mode = MODE_DOT;
        else if (strcmp(argv[i], "--symtab") == 0) mode = MODE_SYMTAB;
        else if (strcmp(argv[i], "--tac") == 0 || strcmp(argv[i], "--ir") == 0) mode = MODE_TAC;
        else if (strcmp(argv[i], "--opt") == 0)    mode = MODE_OPT;
        else if (strcmp(argv[i], "--asm") == 0)    mode = MODE_ASM;
        else if (strcmp(argv[i], "--run") == 0)    mode = MODE_RUN;
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

    /* ---- Preprocessor step: the #include headers ----
     * The lexer records each #include as it scans, so scan the whole file
     * once (the tokens are thrown away) and print the header table. */
    if (mode == MODE_HEADERS) {
        if ((yyin = fopen(input, "r")) == NULL) {
            perror(input);
            return 2;
        }
        while (yylex() != 0)
            ;
        fclose(yyin);
        print_headers(out);
        if (lexer_errors) {
            fflush(out);
            fprintf(stderr, "Compilation failed: %d lexical error(s).\n", lexer_errors);
            return 1;
        }
        return 0;
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
        heading(out, "HEADERS");   /* --all: recorded by the scan above */
        print_headers(out);
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

    if (mode == MODE_DOT) {
        ast_print_dot(program_root, out);
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
    char *tac = generate_tac_text(program_root);
    int status = 0;
    if (mode == MODE_TAC || mode == MODE_ALL) {
        if (mode == MODE_ALL)
            heading(out, "THREE ADDRESS CODE");
        fputs(tac, out);
    }
    if (mode != MODE_TAC)
        status = back_end(tac, &table, mode, out);
    free(tac);

    if (out != stdout)
        fclose(out);
    return status;
}
