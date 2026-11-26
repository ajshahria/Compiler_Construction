/*
 * main.cpp - command-line driver of the MiniJava compiler.
 *
 * Runs the pipeline   source -> lexer -> parser/AST -> semantic analysis
 * (symbol table + type checking) -> three address code   and prints the
 * output of the phase chosen on the command line. Compilation stops after the
 * first phase that reports errors. The back end (backend.cpp) then optimizes
 * the TAC, translates it to x86 assembly or runs it.
 *
 * Exit status: 0 = success, 1 = errors in the program (or a runtime error
 * with --run), 2 = usage/file problem.
 */
#include "ast/ast.h"
#include "backend/backend.h"
#include "codegen/tac.h"
#include "common/diagnostics.h"
#include "imports/imports.h"
#include "semantic/semantic.h"
#include "parser.tab.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern FILE* yyin;
extern char* yytext;
extern int yylineno;
extern int yylex();
extern int yyparse();
extern void yyrestart(FILE* input);
extern Node* syntaxRoot;

namespace {

enum class Mode { Headers, Tokens, Ast, Dot, Symtab, Tac, Opt, Asm, Run, All };

void usage(const char* program) {
    std::cerr << "Usage: " << program << " [option] <source-file> [-o output-file]\n"
                 "Options:\n"
                 "  --headers     list the import lines found (Java's #include)\n"
                 "  --tokens     print the token stream (lexical analysis)\n"
                 "  --ast         print the abstract syntax tree\n"
                 "  --dot         print the abstract syntax tree as a Graphviz graph\n"
                 "                (render it with: dot -Tpng ast.dot -o ast.png)\n"
                 "  --symtab      print the symbol table after semantic analysis\n"
                 "  --tac         print three address code (default)\n"
                 "  --opt         print optimized three address code\n"
                 "  --asm         print x86 assembly (32-bit, AT&T syntax, MinGW)\n"
                 "  --run         run the program (TAC interpreter) and print its output\n"
                 "  --all         print every phase: tokens, imports, AST, symbol table,\n"
                 "                TAC, optimized TAC, assembly and the program output\n"
                 "  --emit-files  also write ast.txt, symbols.txt and output.tac (one file per phase)\n"
                 "  --strict      require the Java form 'class Name { public static void main(...) { ... } }'\n"
                 "                (by default the lab manual's bare statements are accepted too)\n"
                 "  --manual      accept the lab manual's form (the default; kept for compatibility)\n";
}

const char* tokenCategory(int token) {
    switch (token) {
    case IDENT:         return "IDENTIFIER";
    case INT_LITERAL:   return "INT_LITERAL";
    case FLOAT_LITERAL: return "FLOAT_LITERAL";
    case TRUE: case FALSE: return "BOOL_LITERAL";
    case INT: case FLOAT: case BOOLEAN:
    case IF: case ELSE: case WHILE: case FOR: case DO: case SWITCH: case CASE:
    case DEFAULT: case BREAK: case CONTINUE: case RETURN: case PRINT: case PRINTLN:
    case CLASS: case PUBLIC: case STATIC: case VOID: case MAIN: case STRING:
    case SCANNER: case NEW: case SYSTEM_IN:
        return "KEYWORD";
    case SCANNER_READ:  return "METHOD_CALL";
    case LENGTH:        return "ARRAY_LENGTH";
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

/* Back end: reads the TAC with the declared variable types, optimizes it,
 * and prints the optimized TAC, the assembly and/or the program output.
 * --asm and --run use the optimized TAC. Returns the exit status: 1 if the
 * program stopped with a runtime error. */
int backEnd(const std::string& tac, Mode mode, std::ostream& out) {
    std::vector<DeclaredName> declared = declaredVariables();
    std::vector<TacVar> vars;
    for (const DeclaredName& d : declared) {
        TacVar v;
        v.name = d.tacName.c_str();
        v.type = d.type == Type::Float ? TAC_FLOAT : d.type == Type::Bool ? TAC_BOOL : TAC_INT;
        v.length = d.length;
        v.is_function = d.isFunction;
        v.function = d.function.empty() ? nullptr : d.function.c_str();
        vars.push_back(v);
    }
    char error[256];
    TacProgram* program = tac_build(tac.c_str(), vars.data(), static_cast<int>(vars.size()),
                                    error, sizeof error);
    if (!program) {
        std::cerr << "internal error: " << error << '\n';
        return 2;
    }
    tac_optimize(program);

    int status = 0;
    if (mode == Mode::Opt || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "OPTIMIZED THREE ADDRESS CODE");
        char* text = tac_text(program);
        out << text;
        std::free(text);
    }
    if (mode == Mode::Asm || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "X86 ASSEMBLY");
        char* text = tac_assembly(program, "the MiniJava compiler", error, sizeof error);
        if (!text) {
            out.flush();
            std::cerr << error << '\n';
            tac_free(program);
            return 1;
        }
        out << text;
        std::free(text);
    }
    if (mode == Mode::Run || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "PROGRAM OUTPUT");
        const char* runtimeError = nullptr;
        char* text = tac_run(program, &runtimeError);
        out << text;
        std::free(text);
        if (runtimeError) {
            out.flush();
            std::cerr << runtimeError << '\n';
            status = 1;
        }
    }
    tac_free(program);
    return status;
}

}  // namespace

int main(int argc, char* argv[]) {
    Mode mode = Mode::Tac;
    bool emitFiles = false;
    const char* input = nullptr;
    const char* output = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headers")                      mode = Mode::Headers;
        else if (arg == "--tokens")                  mode = Mode::Tokens;
        else if (arg == "--ast")                     mode = Mode::Ast;
        else if (arg == "--dot")                     mode = Mode::Dot;
        else if (arg == "--symtab")                  mode = Mode::Symtab;
        else if (arg == "--tac")                     mode = Mode::Tac;
        else if (arg == "--opt")                     mode = Mode::Opt;
        else if (arg == "--asm")                     mode = Mode::Asm;
        else if (arg == "--run")                     mode = Mode::Run;
        else if (arg == "--all")                     mode = Mode::All;
        else if (arg == "--emit-files")              emitFiles = true;
        else if (arg == "--manual")                  manualMode = true;   // default; kept for old scripts
        else if (arg == "--strict")                  manualMode = false;
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

    // ---- Import step: the lexer records each import line as it scans, so
    // scan the whole file once (the tokens are thrown away) and print them ----
    if (mode == Mode::Headers) {
        if (!(yyin = std::fopen(input, "r"))) { std::perror(input); return 2; }
        while (yylex() != 0) {}
        std::fclose(yyin);
        printImports(out);
        if (lexicalErrors) {
            out.flush();
            std::cerr << "Compilation failed: " << lexicalErrors << " lexical error(s).\n";
            return 1;
        }
        return 0;
    }

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
        heading(out, "IMPORTS");   // --all: recorded by the scan above
        printImports(out);
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

    if (mode == Mode::Dot) {
        printDot(syntaxRoot, out);
        freeTree(syntaxRoot);
        return 0;
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
    std::ostringstream tacText;
    generateTac(syntaxRoot, tacText);
    if (mode == Mode::Tac || mode == Mode::All) {
        if (mode == Mode::All) heading(out, "THREE ADDRESS CODE");
        out << tacText.str();
    }
    int status = mode == Mode::Tac ? 0 : backEnd(tacText.str(), mode, out);

    if (emitFiles) {
        std::ofstream astFile("ast.txt");
        printAst(syntaxRoot, astFile);
        std::ofstream symbolsFile("symbols.txt");
        symbolsFile << symbolTableReport();
        std::ofstream tacFile("output.tac");
        generateTac(syntaxRoot, tacFile);
    }

    freeTree(syntaxRoot);
    return status;
}
