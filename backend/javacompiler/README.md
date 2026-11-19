# MiniJava Compiler

The Java component of the Tricora group project for the Compiler Construction Lab (Metropolitan University, CSE). It is a compiler front-end built with **Flex**, **Bison** and C++. It turns a program into **Three Address Code (TAC)**:

```text
source (.java) -> Lexical Analyzer (Flex) -> tokens
               -> Syntax Analyzer (Bison) -> Abstract Syntax Tree
               -> Semantic Analyzer (symbol table + type checking) -> validated AST
               -> Intermediate Code Generator -> Three Address Code
```

It accepts **the lab manual's language exactly** (section 5): `int`, `float`, `bool`, `if`/`else`, `while`, `print x;` and nested blocks. It also accepts the same program written Java-style: wrapped in `class Name { public static void main(String[] args) { ... } }`, using `boolean` and `System.out.println(x);`. As bonus features (manual section 14) it supports `for`, `do-while`, `switch`/`case`/`default`/`break`, `x++`/`x--`, and a Windows GUI.

As manual section 6 asks, it stops at TAC: no bytecode, assembly or machine code.

## Team

| Member | Responsibility |
|---|---|
| Saima Suchi | Java language compiler (this folder) |
| Abdul Julil Shahria | C++ language compiler and UI (`backend/c++_compiler`) |
| Sabika Rahman | C language compiler (`backend/c-compiler`) |

## Features

- **Lexer**: all keywords, identifiers (letter or `_` first), integer/float/boolean literals, every operator in section 5.3, delimiters, `//` and `/* */` comments. It reports invalid characters, identifiers that start with a digit, and unterminated comments as lexical errors with line numbers.
- **Parser**: a conflict-free Bison grammar with precedence and associativity declarations. Syntax errors are reported with line numbers, and a missing `;` is reported on the line where it belongs. Error recovery skips to the next `;` or `}`, so several errors are reported in one run.
- **AST**: named nodes (`VarDecl`, `Assign`, `Print`, `If`, `IfElse`, `While`, `For`, `Block`, `BinaryOp`, `UnaryOp`, `Identifier`, `Literal`, …). `--ast` prints them as an indented tree.
- **Symbol table**: a stack of scopes. Each entry records name, type, scope id, depth, declaration line and TAC name. `--symtab` prints it.
- **Semantic analysis** reports:
  - undeclared variables, redeclarations and scope violations
  - type mismatches, invalid assignments and invalid expressions
  - non-boolean conditions
  - variables used before initialization (Java's definite-assignment rule)
  - misplaced `break`, and bad `switch` labels
- **TAC**:
  - arithmetic with correct precedence, plus relational and logical expressions, with `&&`/`||` short-circuited
  - control flow (`if`, `if-else`, `while`, `for`, `do-while`, `switch`) using labels and `ifFalse`/`if`/`goto` jumps
  - `print`
  - shadowing variables get their own name (`n@3`)

## Requirements

g++ with C++17 support, GNU Make, Flex and Bison (2.4 or newer). This works with MinGW/MSYS or MSYS2 on Windows, and with `build-essential flex bison` on Ubuntu.

## Build

From this folder (`backend/javacompiler`):

```sh
make
```

This generates the lexer and parser into `build/` and builds `build/minijavac` (`build/minijavac.exe` on Windows). `make clean` removes `build/`.

## Run

```sh
./build/minijavac tests/valid/manual_sample.java             # Three Address Code (default)
./build/minijavac --tokens tests/valid/manual_sample.java    # token stream
./build/minijavac --ast    tests/valid/manual_sample.java    # abstract syntax tree
./build/minijavac --symtab tests/valid/manual_sample.java    # symbol table
./build/minijavac --all    examples/Example.java             # all phases
./build/minijavac examples/Example.java -o out.tac           # write the output to a file
make run                                                     # --all on examples/Example.java
make ui                                                      # build and start the Windows GUI
```

The exit status is `0` when the program compiles, `1` when it has errors, and `2` for a usage or file problem. Errors are printed to standard error as `line N: <lexical|syntax|semantic> error: <description>`.

For example, section 4.6 of the manual

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

This compiles every program in `tests/` and compares the output and exit status with its `.expected` file. If you change the output on purpose, regenerate the expected files with `sh tests/run_tests.sh --update`.

| Folder | What it demonstrates |
|---|---|
| `tests/valid/` | the manual's sample (5.5), its TAC example (4.6), the same sample in Java form, nested scopes with shadowing, `for`, `do-while`, `switch` |
| `tests/lexical/` | invalid character, identifier starting with a digit, unterminated comment |
| `tests/syntax/` | missing semicolon, missing parenthesis, several errors in one file (recovery) |
| `tests/semantic/` | one program per rule in manual 4.5 (undeclared, redeclaration, scope violation, type mismatch, invalid assignment, invalid expression), plus non-boolean condition, uninitialized variable, `break` outside a loop |

## Project layout

```text
docs/
  language-grammar.md   formal CFG of the language
  project-report.md     project report (manual section 12)
src/
  lexer/lexer.l         Flex lexical analyzer
  parser/parser.y       Bison grammar, builds the AST
  ast/                  AST node definition, construction, printer
  symbol_table/         scoped symbol table
  semantic/             scope and type checking
  codegen/              Three Address Code generator
  common/               shared types and error counters
  main.cpp              command-line driver
ui/main.cpp             Windows GUI (editor + one tab per phase, runs the TAC)
tests/                  test programs + .expected outputs + run_tests.sh
examples/               sample program
Makefile                make, make test, make run, make ui, make clean
build/                  generated files and executables (not committed)
```
