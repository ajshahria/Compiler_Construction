#include <fstream>
#include "ast/ast.h"
#include "semantic/semantic.h"
#include "codegen/tac.h"

#include <cstdio>
#include <iostream>

extern FILE* yyin;
extern int yyparse();
extern Node* syntaxRoot;
extern bool hadLexicalError;

int main(int argc, char* argv[]) {

    if (argc != 2) {

        std::cerr << "Usage: minijavac <source.java>\n";

        return 2;
    }

    syntaxRoot = nullptr;
    hadLexicalError = false;

    yyin = fopen(argv[1], "r");

    if (!yyin) {

        std::cerr << "Error: Cannot open file "
                  << argv[1] << '\n';

        return 2;
    }

    std::cout << "=== Mini Java Compiler ===\n";

    // Phase 1 and 2
    std::cout << "\n[1] Lexical and Syntax Analysis\n";

    int result = yyparse();

    fclose(yyin);

    if (result != 0 || syntaxRoot == nullptr) {

        if (hadLexicalError) {
            std::cerr << "\nCompilation failed: Lexical error.\n";
        } else {
            std::cerr << "\nCompilation failed: Syntax error.\n";
        }

        if (syntaxRoot) {
            freeTree(syntaxRoot);
        }

        return 1;
    }

    std::cout << "Syntax analysis successful!\n";

    // Phase 3
    std::cout << "\n[2] Semantic Analysis\n";

    if (!analyze(syntaxRoot)) {

        freeTree(syntaxRoot);

        return 1;
    }

    std::cout << "Semantic analysis successful!\n";

    // Phase 4
    std::cout << "\n[3] Intermediate Code Generation\n";

std::ofstream tacFile("output.tac");

if (!tacFile) {

    std::cerr << "Error: Cannot create output.tac\n";

    freeTree(syntaxRoot);

    return 1;
}

generateTac(syntaxRoot, tacFile);

tacFile.close();

std::cout << "TAC generated successfully!\n";
std::cout << "Output saved in output.tac\n";

    std::cout << "\nCompilation completed successfully.\n";

    freeTree(syntaxRoot);

    return 0;
}
