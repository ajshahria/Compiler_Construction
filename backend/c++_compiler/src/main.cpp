// main.cpp - compiler driver: runs the six phases in order.
//   lexer -> parser (AST) -> semantic (symbol table) -> TAC -> optimizer -> codegen / interpreter
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include "ast.h"
#include "codegen.h"
#include "diagnostics.h"
#include "headers.h"
#include "interpreter.h"
#include "optimizer.h"
#include "semantic.h"
#include "symbol_table.h"
#include "tac.h"

extern FILE* yyin;
extern int yyparse();
extern int g_dumpTokens;
extern BlockNode* g_program;
extern int g_mainCount, g_secondMainLine, g_mainVoidLine;
extern Node* g_mainBlock;

// A C++ program must define 'int main() { ... }' and may only declare variables outside it.
static void checkProgramStructure() {
    if (g_mainCount == 0)
        reportError(PH_SYNTAX, 1, "program has no 'int main()' function: define 'int main() { ... }'");
    if (g_secondMainLine)
        reportError(PH_SYNTAX, g_secondMainLine, "redefinition of 'main'");
    if (g_mainVoidLine)
        reportError(PH_SYNTAX, g_mainVoidLine, "'main' must return int, not void");
    for (size_t i = 0; i < g_program->stmts.size(); i++) {
        Node* s = g_program->stmts[i];
        if (s == g_mainBlock) continue;
        if (s->kind == NodeKind::VarDecl || s->kind == NodeKind::DeclGroup) continue;   // global variables
        if (s->kind == NodeKind::Block && static_cast<BlockNode*>(s)->stmts.empty()) continue;  // stray ';'
        reportError(PH_SYNTAX, s->line, "statement outside of main(): only variable declarations are allowed at the top level");
    }
}

static void usage() {
    std::cout <<
        "Usage: compiler [options] <source-file>\n"
        "  --headers  list the #include headers found (preprocessor step)\n"
        "  --tokens   print the token stream (phase 1)\n"
        "  --ast      print the abstract syntax tree (phase 2)\n"
        "  --symtab   print the symbol table (phase 3)\n"
        "  --tac      print three address code (phase 4)   [default]\n"
        "  --opt      print optimized TAC (phase 5)\n"
        "  --asm      print x86 assembly (phase 6)\n"
        "  --run      execute the program with the TAC interpreter\n"
        "  --all      tokens, ast, symtab, tac, opt, asm and run\n"
        "  -o <file>  write the assembly to <file>\n"
        "  --allow-no-header  do not require #include <iostream>\n"
        "  --allow-no-main    do not require int main() (statements may stand alone)\n"
        "  --manual   same as both flags above: the lab manual's program style\n";
}

int main(int argc, char** argv) {
    bool wHeaders = false, wTokens = false, wAst = false, wSym = false, wTac = false;
    bool wOpt = false, wAsm = false, wRun = false;
    const char* file = NULL;
    const char* asmOut = NULL;
    bool allowNoHeader = false, allowNoMain = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--headers") wHeaders = true;
        else if (a == "--tokens") wTokens = true;
        else if (a == "--ast") wAst = true;
        else if (a == "--symtab") wSym = true;
        else if (a == "--tac") wTac = true;
        else if (a == "--opt") wOpt = true;
        else if (a == "--asm") wAsm = true;
        else if (a == "--run") wRun = true;
        else if (a == "--allow-no-header") allowNoHeader = true;
        else if (a == "--allow-no-main") allowNoMain = true;
        else if (a == "--manual") allowNoHeader = allowNoMain = true;
        else if (a == "--all") wHeaders = wTokens = wAst = wSym = wTac = wOpt = wAsm = wRun = true;
        else if (a == "-o" && i + 1 < argc) { asmOut = argv[++i]; wAsm = true; }
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else file = argv[i];
    }
    if (!file) { usage(); return 1; }
    if (!(wHeaders || wTokens || wAst || wSym || wTac || wOpt || wAsm || wRun)) wTac = true;

    yyin = fopen(file, "r");
    if (!yyin) { std::cerr << "Cannot open file '" << file << "'\n"; return 1; }

    // Phases 1 + 2: the parser pulls tokens from the lexer and builds the AST.
    g_dumpTokens = wTokens;
    if (wTokens) std::cout << "=== Tokens ===\n";
    yyparse();
    fclose(yyin);

    // Every program must include <iostream>, at the top of the file.
    if (!allowNoHeader && !hasHeader("iostream"))
        reportError(PH_LEXICAL, 1, "missing required header: add '#include <iostream>' at the top of the file");

    // Only check the overall shape when the parse itself succeeded (avoids follow-up noise).
    if (!allowNoMain && errorCount(PH_LEXICAL) + errorCount(PH_SYNTAX) == 0)
        checkProgramStructure();

    if (errorCount(PH_LEXICAL) + errorCount(PH_SYNTAX) > 0) {
        std::cerr << "Compilation failed: " << errorCount(PH_LEXICAL) << " lexical and "
                  << errorCount(PH_SYNTAX) << " syntax error(s).\n";
        return 1;
    }
    if (wHeaders) { std::cout << "=== Headers ===\n"; printHeaders(std::cout); }
    if (wAst) { std::cout << "=== AST ===\n"; g_program->print(std::cout, 0); }

    // Phase 3: semantic analysis with the symbol table.
    SymbolTable table;
    int semErrors = analyze(g_program, table);
    if (wSym) { std::cout << "=== Symbol Table ===\n"; table.dump(std::cout); }
    if (semErrors > 0) {
        std::cerr << "Compilation failed: " << semErrors << " semantic error(s).\n";
        return 1;
    }

    // Phase 4: intermediate code.
    TacProgram tac = generateTAC(g_program, table);
    if (wTac) { std::cout << "=== Three Address Code ===\n"; printTAC(tac, std::cout); }

    // Phase 5: optimization.
    TacProgram opt = optimize(tac);
    if (wOpt) { std::cout << "=== Optimized TAC ===\n"; printTAC(opt, std::cout); }

    // Phase 6: code generation (+ interpreter to run the result).
    if (wAsm) {
        std::string text = generateAssembly(opt);
        if (asmOut) {
            std::ofstream f(asmOut);
            f << text;
            std::cout << "Assembly written to " << asmOut << "\n";
        } else {
            std::cout << "=== x86 Assembly ===\n" << text;
        }
    }
    if (wRun) {
        std::cout << "=== Program Output ===\n";
        if (!interpret(opt, std::cout, std::cin)) return 2;
    }
    return 0;
}
