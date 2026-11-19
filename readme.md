# Compiler Construction Project

This repository is a university course project for Compiler Construction. The goal of this project is to build a simple compiler system capable of compiling C, C++, and Java programs through the main phases of a compiler: lexical analysis, syntax analysis, semantic analysis, intermediate code generation, optimization, and code generation.

## Project Overview

A compiler translates source code written in a high-level programming language into executable or intermediate code. In this project, we aim to understand how compilers work by implementing a simplified version for three widely used languages: C, C++, and Java.

The main objectives of this project are to:

- Understand the compilation process
- Implement lexical and syntactic analysis for multiple languages
- Build semantic checks and symbol tables
- Generate intermediate code for compiled programs
- Develop a simple user interface for compiler interaction
- Apply compiler design concepts in a practical university project

## Core Compiler Phases

### 1. Lexical Analysis
The lexer reads source code and converts it into tokens such as keywords, identifiers, numbers, operators, and symbols.

### 2. Syntax Analysis
The parser verifies whether the token sequence follows the grammar of the language and generates a parse tree or abstract syntax tree (AST).

### 3. Semantic Analysis
This phase checks variable declarations, type compatibility, scope rules, and other language constraints.

### 4. Intermediate Code Generation
The compiler converts the AST into an intermediate representation (IR) to simplify further processing.

### 5. Optimization
The optimization phase reduces redundant operations and improves the efficiency of the generated code.

### 6. Code Generation
The final phase produces target code that can be executed or translated into machine-level instructions.

## Project Scope

This project focuses on creating a simple compiler environment that supports:

- C language compiler
- C++ language compiler
- Java language compiler
- A user interface for input and result display

The system will support basic compilation tasks and demonstrate the core working principles of compiler construction.

## Team Members and Responsibilities

- Abdul Julil Shahria - Responsible for the C++ language compiler and UI
- Saima Suchi - Responsible for the Java language compiler
- Sabika Rahman - Responsible for the C language compiler

## Suggested Development Workflow

1. Define the language grammar
2. Implement the lexer
3. Build the parser
4. Create the AST
5. Add semantic analysis
6. Generate intermediate code
7. Run tests and fix errors
8. Optimize and finalize code generation

## Getting Started

### Prerequisites

- A modern C++/Java/Python environment, depending on the implementation language
- A compiler or interpreter for the selected language
- A build tool such as CMake, Make, or Gradle

### Example

```bash
git clone https://github.com/yourusername/compiler-construction-project.git
cd compiler-construction-project
# build the project
# run the compiler on an input program
```

## Example Compilation Workflow

```text
source code -> lexer -> parser -> AST -> semantic analysis -> IR -> optimization -> code generation -> target code
```

## Learning Goals

This project helps build practical knowledge in:

- Formal languages and grammar
- Tokenization and parsing
- AST design
- Symbol tables
- Type checking and scope resolution
- Code generation concepts
- Compiler error handling
- Multi-language compiler design

## Contribution

This project is developed as part of the Compiler Construction course. Team members can continue improving the lexer, parser, semantic analyzer, and compiler interface as the project expands.

## License

This project is intended for educational and university coursework purposes.

## Notes

This repository is planned as a simple but functional compiler construction project for academic learning and demonstration.

---

# Tricora Compiler - C++ version (branch `c++_compiler`)

A Flex/Bison compiler written in C++ for the fixed mini language of the lab manual
(`int`/`float`/`bool`, `if`/`else`, `while`, `print`, nested scopes) plus C++-style extras.
It implements all six phases and ships a small UI.

## Build and run

```bash
cd backend/c++_compiler                # all C++ compiler files live here
make                                   # builds build/compiler.exe (needs g++, flex, bison)
build/compiler.exe examples/headers_demo.cpp # prints TAC (default)
build/compiler.exe --manual --all examples/sample.cpp   # the manual's sample needs --manual
make test                              # runs every program in tests/ against its .expected file
python ui/compiler_ui.py               # GUI: editor + one tab per phase
```

| Flag | Output |
|---|---|
| `--tokens` | token stream (phase 1, lexical) |
| `--ast` | indented AST (phase 2, syntax) |
| `--symtab` | symbol table: name, type, scope id, line (phase 3, semantic) |
| `--tac` | three address code (phase 4) — default |
| `--opt` | optimized TAC (phase 5) |
| `--asm` / `-o f.s` | 32-bit x86 AT&T assembly (phase 6); build with `gcc f.s -o f.exe` (MinGW) |
| `--run` | executes the program with the TAC interpreter |

## Phases and files

| Phase | Files |
|---|---|
| 1 Lexical analysis | `src/lexer/lexer.l` |
| 2 Syntax analysis + AST | `src/parser/parser.y`, `src/ast/ast.h`, `src/ast/ast.cpp` |
| 3 Semantic analysis | `src/semantic/semantic.*`, `src/symbol_table/symbol_table.*` |
| 4 Intermediate code (TAC) | `src/tac/tac.*` |
| 5 Optimization (folding, propagation, dead code) | `src/optimizer/optimizer.*` |
| 6 Code generation | `src/codegen/codegen.*`, `src/codegen/assembly.*`, `src/interpreter/interpreter.*` |
| Driver / errors | `src/main.cpp`, `src/common/` |

Formal grammar: [`backend/c++_compiler/docs/grammar.md`](backend/c++_compiler/docs/grammar.md). Test programs: `backend/c++_compiler/tests/{valid,lexical,syntax,semantic}`. Full guide: [`README_CPP.md`](backend/c++_compiler/README_CPP.md).

## C++ extras (bonus)

`int main()`, `cout << "text" << x << endl;`, `cin >>`, `for`, `do-while`, `switch`, `break`/`continue`, `?:`, `x++`, `+=` and more — see `README_CPP.md` for the full list.

## Header files (`#include`)

The lexer recognises `#include <header>` lines and `using namespace std;` (`src/preprocessor/headers.*`).
All C++ standard headers (`<iostream>`, `<vector>`, `<cmath>`, `<cstdlib>`, ... up to C++23, plus `<stdio.h>`-style C headers
and `<bits/stdc++.h>`) are accepted and listed with `--headers`. Rules:

- Every program must start with `#include <iostream>` (error `missing required header` otherwise), and `#include` lines must be at the top of the file. `--allow-no-header` relaxes the first rule. Every program must also define `int main()` (error otherwise); `--manual` relaxes both rules for the lab manual sample.
- `cout << x << endl;` (or `std::cout`) needs `<iostream>`.
- An unknown header (`<notaheader>`) or a quoted user header (`"file.h"`) is a lexical error with its line number.
- Other headers are accepted but have no effect, because the language has no library calls yet.
