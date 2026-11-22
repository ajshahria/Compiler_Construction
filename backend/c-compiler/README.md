# C Compiler

A compiler front-end for the lab manual's mini language (`.tri` files), written in C with Flex and Bison. A program may be written exactly as in the manual, or as ordinary C (bonus): `#include <stdio.h>`, `int main() { ... }`, `return` and `printf("...", ...)` with `%d %i %f %.Nf %g %%` (see [`docs/grammar.md`](docs/grammar.md)). Its front end produces Three Address Code, the output the manual requires; an extra back end can then optimize the TAC, translate it to x86 assembly, or run it:

```text
source (.tri) -> lexer -> parser/AST -> semantic analysis (symbol table) -> Three Address Code
                -> back end: optimized TAC (--opt), x86 assembly (--asm), program output (--run)
```

## Directory contents

```text
c-compiler/
├── Makefile                     make, make test, make clean
├── README.md                    this file
├── .vscode/
│   ├── settings.json            editor settings
│   └── tasks.json               "Build" and "Run example" tasks
├── docs/
│   ├── grammar.md               formal CFG, precedence table, lexical rules
│   └── PROJECT_REPORT.md        project report
├── src/
│   ├── main.c                   command-line driver, runs the phases in order
│   ├── lexer/lexer.l            Flex lexer: tokens, comments, lexical errors
│   ├── parser/parser.y          Bison grammar, builds the AST, error recovery
│   ├── ast/ast.{h,c}            AST nodes, constructors and the --ast printer
│   ├── symbol_table/symbol_table.{h,c}   stack of scopes: name, type, scope, depth, line, TAC name
│   ├── semantic/semantic.{h,c}  scope and type checking
│   ├── codegen/codegen.{h,c}    Three Address Code generation
│   └── backend/backend.{h,c}    back end: optimizer (--opt), interpreter (--run), x86 assembly (--asm)
├── examples/
│   ├── valid/                   control.tri, manual_sample.tri, types.tri
│   └── invalid/                 type_error.tri, undeclared.tri
└── tests/
    ├── run_tests.sh             runs every test and compares it with its .expected file
    ├── valid/                   15 programs that compile (9 of them test --opt, --run or --asm)
    ├── lexical/                 5 programs with lexical errors
    ├── syntax/                  3 programs with syntax errors
    └── semantic/                11 programs with semantic errors
```

`make` writes everything it generates to `build/` (`parser.tab.c`, `parser.tab.h`, `lex.yy.c` and `tricora.exe`). That folder is not committed.

## Requirements

`gcc`, `make`, `flex` and `bison` (Bison 2.4 or newer). MinGW/MSYS, MSYS2 UCRT64 and Linux all work.

## Build

```sh
make
```

## Run

```sh
./build/tricora examples/valid/manual_sample.tri             # Three Address Code (default)
./build/tricora --all examples/valid/manual_sample.tri       # every phase
./build/tricora examples/valid/manual_sample.tri -o out.tac  # write the output to a file
```

| Option | Output |
|---|---|
| `--tokens` | token stream |
| `--ast` | abstract syntax tree |
| `--symtab` | symbol table |
| `--tac` | Three Address Code (default; `--ir` also works) |
| `--opt` | optimized Three Address Code (constant propagation and folding, unreachable code, dead temporaries) |
| `--asm` | x86 assembly (32-bit, AT&T syntax; assemble with MinGW `gcc out.s -o out.exe`) |
| `--run` | run the program with the TAC interpreter and print its output |
| `--all` | all of the above |
| `-o file` | write the output to a file |

Exit status: `0` compiled, `1` errors in the program (or, with `--run`, a runtime error such as division by zero), `2` usage or file problem. Errors are printed as `line N: <lexical|syntax|semantic> error: <description>`.

## Tests

```sh
make test                        # run every test
sh tests/run_tests.sh --update   # regenerate the .expected files after an intended change
```

Each `tests/<category>/<name>.tri` has a matching `<name>.expected` file. Its first line is the exit status; the rest is the compiler's output. A test can pass compiler options on its first line (`// flags: --run`).

| Folder | Programs |
|---|---|
| `valid/` | arithmetic, asm_printf, asm_small, c_printf, control, manual_sample, manual_tac_example, nested_scopes, opt_constant_folding, run_c_crlf_includes, run_c_hello, run_c_printf, run_division_by_zero, run_manual_sample, run_types_and_scopes |
| `lexical/` | c_bad_header, c_unterminated_string, invalid_character, invalid_identifier, unterminated_comment |
| `syntax/` | missing_parenthesis, missing_semicolon, multiple_errors |
| `semantic/` | c_main_errors, c_printf_errors, c_return_outside_main, invalid_assignment, invalid_expression, multiple_errors, non_bool_condition, redeclaration, scope_violation, type_mismatch, undeclared |
