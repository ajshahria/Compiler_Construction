/*
 * main.cpp - command-line driver of the MiniJava compiler.
 *
 * Runs the pipeline   source -> lexer -> parser/AST -> semantic analysis
 * (symbol table + type checking) -> three address code   and prints the
 * output of the phase chosen on the command line. Compilation stops after the
 * first phase that reports errors.
 *
 * Exit status: 0 = success, 1 = errors in the program, 2 = usage/file problem.
 */
#include "ast/ast.h"
#include "codegen/tac.h"
#include "common/diagnostics.h"
#include "semantic/semantic.h"
#include "parser.tab.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

extern FILE* yyin;
extern char* yytext;
extern int yylineno;
extern int yylex();
extern int yyparse();
extern void yyrestart(FILE* input);
extern Node* syntaxRoot;

namespace {

enum class Mode { Tokens, Ast, Symtab, Tac, All };

void usage(const char* program) {
    std::cerr << "Usage: " << program << " [option] <source-file> [-o output-file]\n"
                 "Options:\n"
                 "  --tokens      print the token stream (lexical analysis)\n"
                 "  --ast         print the abstract syntax tree\n"
                 "  --symtab      print the symbol table after semantic analysis\n"
                 "  --tac         print three address code (default)\n"
                 "  --all         print tokens, AST, symbol table and TAC\n"
                 "  --emit-files  also write ast.txt, symbols.txt and output.tac (one file per phase)\n"
                 "  --manual      also accept a program without a class, exactly as in the lab manual\n";
}

const char* tokenCategory(int token) {
    switch (token) {
    case IDENT:         return "IDENTIFIER";
    case INT_LITERAL:   return "INT_LITERAL";
    case FLOAT_LITERAL: return "FLOAT_LITERAL";
    case TRUE: case FALSE: return "BOOL_LITERAL";
    case INT: case FLOAT: case BOOLEAN:
    case IF: case ELSE: case WHILE: case FOR: case DO: case SWITCH: case CASE:
    case DEFAULT: case BREAK: case PRINT: case PRINTLN:
    case CLASS: case PUBLIC: case STATIC: case VOID: case MAIN: case STRING:
        return "KEYWORD";
    case EQ: case NE: case LE: case GE: return "RELATIONAL_OP";
    case AND: case OR:                  return "LOGICAL_OP";
    case INC: case DEC:                 return "INCREMENT_OP";
    }
    switch (token) {
    case '+': case '-': case '*': case '/': case '%': return "ARITHMETIC_OP";
    case '<': case '>':                               return "RELATIONAL_OP";
    case '!':                                         return "LOGICAL_OP";
    case '=':                                         return "ASSIGN";
    }
    return "DELIMITER";
}

void printTokens(std::ostream& out) {
    out << std::left << std::setw(6) << "Line" << std::setw(16) << "Token" << "Lexeme\n";
    int token;
    while ((token = yylex()) != 0)
        out << std::setw(6) << yylloc.first_line << std::setw(16) << tokenCategory(token) << yytext << '\n';
}

void heading(std::ostream& out, const char* title) {
    out << "===== " << title << " =====\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    Mode mode = Mode::Tac;
    bool emitFiles = false;
    const char* input = nullptr;
    const char* output = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--tokens")                       mode = Mode::Tokens;
        else if (arg == "--ast")                     mode = Mode::Ast;
        else if (arg == "--symtab")                  mode = Mode::Symtab;
        else if (arg == "--tac")                     mode = Mode::Tac;
        else if (arg == "--all")                     mode = Mode::All;
        else if (arg == "--emit-files")              emitFiles = true;
        else if (arg == "--manual")                  manualMode = true;
        else if (arg == "-o" && i + 1 < argc)        output = argv[++i];
        else if (arg[0] != '-' && input == nullptr)  input = argv[i];
        else { usage(argv[0]); return 2; }
    }
    if (!input) { usage(argv[0]); return 2; }

    std::ofstream file;
    if (output) {
        file.open(output);
        if (!file) { std::cerr << "Error: cannot create " << output << '\n'; return 2; }
    }
    std::ostream& out = output ? file : std::cout;

    // ---- Phase 1: lexical analysis (token listing only) ----
    if (mode == Mode::Tokens || mode == Mode::All) {
        if (!(yyin = std::fopen(input, "r"))) { std::perror(input); return 2; }
        if (mode == Mode::All) heading(out, "TOKENS");
        printTokens(out);
        std::fclose(yyin);
        if (lexicalErrors) {
            out.flush();
            std::cerr << "Compilation failed: " << lexicalErrors << " lexical error(s).\n";
            return 1;
        }
        if (mode == Mode::Tokens) return 0;
        yylineno = 1;   // --all: scan the file again for the parser
    }

    // ---- Phase 2: syntax analysis, builds the AST ----
    if (!(yyin = std::fopen(input, "r"))) { std::perror(input); return 2; }
    yyrestart(yyin);
    yyparse();
    std::fclose(yyin);

    if (lexicalErrors || syntaxErrors || !syntaxRoot) {
        out.flush();
        std::cerr << "Compilation failed: " << lexicalErrors << " lexical error(s), "
                  << syntaxErrors << " syntax error(s).\n";
        freeTree(syntaxRoot);
        return 1;
    }

    if (mode == Mode::Ast || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "ABSTRACT SYNTAX TREE");
        printAst(syntaxRoot, out);
        if (mode == Mode::Ast) { freeTree(syntaxRoot); return 0; }
    }

    // ---- Phase 3: semantic analysis with the symbol table ----
    int semanticErrors = analyze(syntaxRoot);
    if (semanticErrors) {
        out.flush();
        std::cerr << "Compilation failed: " << semanticErrors << " semantic error(s).\n";
        freeTree(syntaxRoot);
        return 1;
    }

    if (mode == Mode::Symtab || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "SYMBOL TABLE");
        out << symbolTableReport();
        if (mode == Mode::Symtab) { freeTree(syntaxRoot); return 0; }
    }

    // ---- Phase 4: intermediate code generation ----
    if (mode == Mode::All) heading(out, "THREE ADDRESS CODE");
    generateTac(syntaxRoot, out);

    if (emitFiles) {
        std::ofstream astFile("ast.txt");
        printAst(syntaxRoot, astFile);
        std::ofstream symbolsFile("symbols.txt");
        symbolsFile << symbolTableReport();
        std::ofstream tacFile("output.tac");
        generateTac(syntaxRoot, tacFile);
    }

    freeTree(syntaxRoot);
    return 0;
}
