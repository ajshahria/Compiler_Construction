# C++ Compiler

A compiler for the lab manual's mini language written in C++ style (`#include <iostream>`, `int main()`, `cout`, `cin`, `for`, `switch`, …). It is built with Flex, Bison and C++ and runs all six phases:

```text
source (.cpp) -> lexer -> parser/AST -> semantic analysis -> TAC -> optimized TAC -> x86 assembly
                                                                                  -> TAC interpreter (--run)
```

## Directory contents

```text
c++_compiler/
├── Makefile                     make, make test, make clean
├── README.md                    this file
├── docs/
│   └── grammar.md               formal grammar of the language
├── src/
│   ├── main.cpp                 command-line driver, runs the phases in order
│   ├── lexer/lexer.l            Flex lexer: tokens, comments, #include lines, lexical errors
│   ├── preprocessor/headers.*   known C/C++ headers and the #include rules
│   ├── parser/parser.y          Bison grammar, builds the AST, error recovery
│   ├── ast/ast.*                AST node classes and the --ast printer
│   ├── symbol_table/symbol_table.*   stack of scopes: name, type, scope, line, TAC name
│   ├── semantic/semantic.*      scope and type checking
│   ├── tac/tac.*                Three Address Code generation
│   ├── optimizer/optimizer.*    constant folding/propagation, dead and unreachable code removal
│   ├── codegen/codegen.*        TAC -> 32-bit x86 assembly (AT&T, MinGW)
│   ├── codegen/assembly.*       assembly text helpers
│   ├── interpreter/interpreter.*   runs TAC directly (--run)
│   └── common/                  types.h, diagnostics.* (error messages and counts)
├── examples/
│   ├── sample.cpp               the manual's sample program (section 5.5)
│   ├── floats.cpp               float arithmetic, int/float mixing, shadowing
│   ├── cpp_extras.cpp           for, ++/--, compound assignment, cout
│   ├── headers_demo.cpp         standard headers, using namespace std, endl
│   └── demo_programs.py         writes build/demo.html: several programs, one tab per phase
└── tests/
    ├── run_tests.sh             runs every test and compares it with its .expected file
    ├── valid/                   23 programs that compile and run
    ├── lexical/                 7 programs with lexical errors
    ├── syntax/                  9 programs with syntax errors
    └── semantic/                9 programs with semantic errors
```

`make` writes everything it generates to `build/` (the parser and lexer sources, object files, `compiler.exe`). That folder is not committed.

## Requirements

`g++` (C++11 or newer), `flex`, `bison` and `make`. On Windows, use MinGW/MSYS and run the commands in Git Bash. Tested with GCC 6.3, Flex 2.5.35, Bison 2.4.2 and GNU Make 3.81.

## Build

```bash
make
```

## Run

```bash
build/compiler.exe examples/headers_demo.cpp               # Three Address Code (default)
build/compiler.exe --manual --all examples/sample.cpp      # every phase (manual-style program)
build/compiler.exe --opt --run examples/floats.cpp         # optimized TAC, then run it
build/compiler.exe --manual -o build/out.s examples/sample.cpp   # write x86 assembly
gcc build/out.s -o build/out.exe                           # assemble it (MinGW)
python examples/demo_programs.py                           # build/demo.html
```

| Option | Output |
|---|---|
| `--headers` | `#include` headers found |
| `--tokens` | token stream |
| `--ast` | indented syntax tree |
| `--symtab` | symbol table (name, type, scope, line) |
| `--tac` | Three Address Code (default) |
| `--opt` | optimized TAC |
| `--asm` | x86 assembly |
| `--run` | execute the program with the TAC interpreter |
| `--all` | all of the above |
| `-o file.s` | write the assembly to a file |
| `--allow-no-header` | do not require `#include <iostream>` |
| `--allow-no-main` | do not require `int main()` |
| `--manual` | both of the above, for programs written exactly in the manual's style |

The two rules these options relax:
- A program must start with `#include <iostream>`, and `#include` lines must come before any code.
- A program must define `int main() { ... }`. Only variable declarations may appear outside it.

Errors are printed as `<Phase> error [line N]: message`. Compilation stops after the first phase that reports errors.

## Tests

```bash
make test                         # run every test
bash tests/run_tests.sh --update  # regenerate the .expected files after an intended change
```

Each `tests/<category>/<name>.cpp` has a matching `<name>.expected` file containing the expected output. A test can set compiler flags on its first line (`// flags: --allow-no-header`) and give `cin` input (`// stdin: 2 5`).

| Folder | Programs |
|---|---|
| `valid/` | beginner_main, cin_float_bool, cpp_extras, fibonacci, floats_scopes, gcd_palindrome, global_variable, harmonic_ternary, headers_iostream, indented_include, loops_switch_bool, manual_sample_no_header, nested_control, primes, sample_manual, switch_continue_logic, temperature_collatz, textbook_for_loop, textbook_hello, textbook_if_else, textbook_sum, textbook_using_hello, textbook_using_sum |
| `lexical/` | bad_identifier, bad_string, include_after_code, invalid_char, missing_header, unknown_header, unterminated_comment |
| `syntax/` | bad_main, main_without_braces, main_without_type, missing_semicolon, no_main, statement_outside_main, two_mains, unbalanced, void_main |
| `semantic/` | cout_without_iostream, invalid_assignment, invalid_expression, jump_errors, redeclaration, scope_violation, ternary_error, type_mismatch, undeclared |
