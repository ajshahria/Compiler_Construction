# MiniJava Compiler

A compiler front-end for the lab manual's mini language, written in C++ with Flex and Bison. A program is either a bare list of statements, exactly as in the manual's sample program, or a `class` with `main` (`--strict` accepts only the class form); inside it, statements can be written exactly as in the manual (`bool`, `print x;`) or in Java style (`boolean`, `System.out.println(x);`). Its front end produces Three Address Code, the output the manual requires; an extra back end can then optimize the TAC, translate it to x86 assembly, or run it:

```text
source (.java) -> lexer -> parser/AST -> semantic analysis (symbol table) -> Three Address Code
                 -> back end: optimized TAC (--opt), x86 assembly (--asm), program output (--run)
```

## Directory contents

```text
javacompiler/
├── Makefile                     make, make test, make run, make clean
├── README.md                    this file
├── docs/
│   ├── language-grammar.md      formal CFG, precedence table, lexical rules
│   └── project-report.md        project report
├── src/
│   ├── main.cpp                 command-line driver, runs the phases in order
│   ├── lexer/lexer.l            Flex lexer: tokens, comments, lexical errors
│   ├── parser/parser.y          Bison grammar, builds the AST, error recovery
│   ├── ast/ast.{h,cpp}          AST node, construction and the --ast printer
│   ├── symbol_table/symbol_table.{h,cpp}   stack of scopes: type, line, scope, TAC name, initialized
│   ├── semantic/semantic.{h,cpp}   scope, type and definite-initialization checks
│   ├── codegen/tac.{h,cpp}      Three Address Code generation
│   ├── backend/backend.{h,cpp}  back end: optimizer (--opt), interpreter (--run), x86 assembly (--asm)
│   └── common/                  types.h, diagnostics.h (shared error counters)
├── examples/
│   └── Example.java             sample program in Java style
└── tests/
    ├── run_tests.sh             runs every test and compares it with its .expected file
    ├── valid/                   15 programs that compile (5 of them test --opt, --run or --asm)
    ├── lexical/                 3 programs with lexical errors
    ├── syntax/                  4 programs with syntax errors
    └── semantic/                9 programs with semantic errors
```

`make` writes everything it generates to `build/` (`parser.tab.cpp`, `parser.tab.hpp`, `lex.yy.cpp`, and `minijavac.exe`). That folder is not committed.

## Requirements

`g++` with C++17, `make`, `flex` and `bison` (Bison 2.4 or newer).

## Build

```sh
make
```

## Run

```sh
./build/minijavac tests/valid/manual_sample.java        # Three Address Code (default)
./build/minijavac tests/valid/manual_bare_sample.java   # the manual's sample, no class
./build/minijavac --all examples/Example.java           # every phase
./build/minijavac examples/Example.java -o out.tac      # write the output to a file
make run                                                # --all on examples/Example.java
```

| Option | Output |
|---|---|
| `--tokens` | token stream |
| `--ast` | abstract syntax tree |
| `--symtab` | symbol table |
| `--tac` | Three Address Code (default) |
| `--opt` | optimized Three Address Code (constant propagation and folding, unreachable code, dead temporaries) |
| `--asm` | x86 assembly (32-bit, AT&T syntax; assemble with MinGW `gcc out.s -o out.exe`) |
| `--run` | run the program with the TAC interpreter and print its output |
| `--all` | all of the above |
| `-o file` | write the output to a file |
| `--strict` | accept only the `class Name { public static void main(...) { ... } }` form; a bare list of statements is a syntax error |
| `--manual` | accept a program without a class, exactly as in the lab manual's sample (section 5.5); this is the default and the flag is kept for compatibility |
| `--emit-files` | also write `ast.txt`, `symbols.txt` and `output.tac` (one file per phase) |

Exit status: `0` compiled, `1` errors in the program (or, with `--run`, a runtime error such as division by zero), `2` usage or file problem. Errors are printed as `line N: <lexical|syntax|semantic> error: <description>`.

## Tests

```sh
make test                        # run every test
sh tests/run_tests.sh --update   # regenerate the .expected files after an intended change
```

Each `tests/<category>/<name>.java` has a matching `<name>.expected` file. Its first line is the exit status; the rest is the compiler's output. A test can pass compiler options on its first line (`// flags: --strict`).

| Folder | Programs |
|---|---|
| `valid/` | asm_small, do_while, for_loop, java_wrapper, manual_bare_sample, manual_sample, manual_tac_example, nested_scopes, opt_constant_folding, run_division_by_zero, run_manual_bare_sample, run_stress, smoke, sum_three, switch_case |
| `lexical/` | bad_character, invalid_identifier, unterminated_comment |
| `syntax/` | missing_class, missing_parenthesis, missing_semicolon, multiple_errors |
| `semantic/` | break_outside_loop, invalid_assignment, invalid_expression, non_boolean_condition, redeclaration, scope_violation, type_mismatch, undeclared, uninitialized |
