# Tricora C Compiler

The C component of the Tricora group project for the Compiler Construction Lab (Metropolitan University, CSE). It is a compiler front-end for the mini language fixed by the lab manual (section 5), built with **Flex** and **Bison** in C. It turns a source program into **Three Address Code (TAC)**, going through every required phase:

```text
source (.tri) -> Lexical Analyzer (Flex) -> tokens
              -> Syntax Analyzer (Bison) -> Abstract Syntax Tree
              -> Semantic Analyzer (symbol table + type checking) -> validated AST
              -> Intermediate Code Generator -> Three Address Code
```

Following manual section 6, it stops at TAC: it does not generate machine code or assembly.

## Team

| Member | Responsibility |
|---|---|
| Sabika Rahman | C language compiler (this folder) |
| Abdul Julil Shahria | C++ language compiler and UI (`backend/c++_compiler`) |
| Saima Suchi | Java language compiler |

## Features

- **Lexer**: keywords `int float bool if else while print true false`, identifiers, integer/float/bool literals, all operators of section 5.3, delimiters `{ } ( ) ;`. `//` and `/* */` comments and all whitespace are discarded. Invalid characters, identifiers that start with a digit, and unterminated comments are reported as lexical errors with line numbers.
- **Parser**: an unambiguous Bison grammar (no conflicts) with precedence and associativity declarations. Syntax errors are reported with line numbers; a missing `;` is reported on the line where it belongs. Error recovery skips to the next `;` or `}`, so several errors are reported in one run.
- **AST**: one node type per construct (`VarDecl`, `Assign`, `Print`, `If`, `IfElse`, `While`, `Block`, `BinaryOp`, `UnaryOp`, `Identifier`, literals), printed as an indented tree with `--ast`.
- **Symbol table**: a stack of scopes, where each `{ }` block opens a new one. Each entry records name, type, scope id, depth and declaration line. It is printed with `--symtab`.
- **Semantic analysis**: detects undeclared variables, redeclarations, scope violations, type mismatches, invalid assignments, invalid expressions and non-bool `if`/`while` conditions.
- **TAC generation**: arithmetic with correct precedence, relational and logical expressions, `if`, `if-else`, `while` with labels and `ifFalse`/`goto` jumps, and `print`. A variable that shadows an outer one gets its own TAC name (`n@3`).

## Requirements

GCC, GNU Make, Flex and Bison (Bison 2.4 or newer). Any of these toolchains works:

- **Windows, MinGW/MSYS** (the toolchain used during development): `gcc`, `make`, `flex`, `bison` on `PATH`.
- **Windows, MSYS2 UCRT64**: `pacman -S --needed base-devel mingw-w64-ucrt-x86_64-gcc flex bison`.
- **Ubuntu/Debian**: `sudo apt install build-essential flex bison`.

## Build

From this folder (`backend/c-compiler`):

```sh
make
```

This generates `build/parser.tab.c`, `build/parser.tab.h` and `build/lex.yy.c`, then builds `build/tricora` (`build/tricora.exe` on Windows). `make clean` removes the `build/` folder.

## Run

```sh
./build/tricora examples/valid/manual_sample.tri            # Three Address Code (default)
./build/tricora --tokens examples/valid/manual_sample.tri   # token stream
./build/tricora --ast    examples/valid/manual_sample.tri   # abstract syntax tree
./build/tricora --symtab examples/valid/manual_sample.tri   # symbol table
./build/tricora --all    examples/valid/manual_sample.tri   # all of the above
./build/tricora examples/valid/manual_sample.tri -o out.tac # write the output to a file
```

The exit status is `0` when the program compiles, `1` when it has errors, and `2` for a usage or file problem. Error messages go to standard error in the form `line N: <kind> error: <description>`.

Example: the manual's section 4.6 program

```text
int a;  int b;  int c;
a = 5;  b = 10;  c = a + b * 2;
```

produces

```text
a = 5
b = 10
t1 = b * 2
t2 = a + t1
c = t2
```

## Tests

```sh
make test
```

This runs every program in `tests/` and compares the compiler's output and exit status with the matching `.expected` file. If you change the compiler's output on purpose, regenerate the expected files with `sh tests/run_tests.sh --update`.

| Folder | What it demonstrates |
|---|---|
| `tests/valid/` | Programs that compile to TAC: the manual's sample (5.5), the manual's TAC example (4.6), arithmetic, loops/branches, nested scopes with shadowing |
| `tests/lexical/` | Invalid character, identifier starting with a digit, unterminated comment |
| `tests/syntax/` | Missing semicolon, missing parenthesis, several errors in one file (recovery) |
| `tests/semantic/` | One program per rule in manual 4.5: undeclared, redeclaration, scope violation, type mismatch, invalid assignment, invalid expression; also a non-bool condition and several errors in one file |

## Project layout

```text
docs/
  grammar.md          formal CFG of the language
  PROJECT_REPORT.md   project report (manual section 12)
src/
  lexer/lexer.l       Flex lexical analyzer
  parser/parser.y     Bison grammar, builds the AST
  ast/                AST node definitions, constructors, printer
  symbol_table/       scoped symbol table
  semantic/           scope and type checking
  codegen/            Three Address Code generator
  main.c              command-line driver
tests/                test programs + .expected outputs + run_tests.sh
examples/             sample programs (valid/ and invalid/)
Makefile              `make`, `make test`, `make clean`
build/                generated files and the executable (not committed)
```
