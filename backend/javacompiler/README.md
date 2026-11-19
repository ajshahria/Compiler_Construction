# MiniJava Compiler

A compiler front-end for the lab manual's mini language, written in C++ with Flex and Bison. A program can be written exactly as in the manual (`bool`, `print x;`) or in Java style, as a `class` with `main`, `boolean` and `System.out.println(x);`. It stops at Three Address Code:

```text
source (.java) -> lexer -> parser/AST -> semantic analysis (symbol table) -> Three Address Code
```

## Directory contents

```text
javacompiler/
├── Makefile                     make, make test, make run, make ui, make clean
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
│   └── common/                  types.h, diagnostics.h (shared error counters)
├── examples/
│   └── Example.java             sample program in Java style
├── tests/
│   ├── run_tests.sh             runs every test and compares it with its .expected file
│   ├── valid/                   9 programs that compile to TAC
│   ├── lexical/                 3 programs with lexical errors
│   ├── syntax/                  3 programs with syntax errors
│   └── semantic/                9 programs with semantic errors
└── ui/
    └── main.cpp                 Windows GUI: editor, one tab per phase, runs the TAC
```

`make` writes everything it generates to `build/` (`parser.tab.cpp`, `parser.tab.hpp`, `lex.yy.cpp`, `minijavac.exe` and `compiler_ui.exe`). That folder is not committed.

## Requirements

`g++` with C++17, `make`, `flex` and `bison` (Bison 2.4 or newer). The GUI is Windows only.

## Build

```sh
make
```

## Run

```sh
./build/minijavac tests/valid/manual_sample.java        # Three Address Code (default)
./build/minijavac --all examples/Example.java           # every phase
./build/minijavac examples/Example.java -o out.tac      # write the output to a file
make run                                                # --all on examples/Example.java
make ui                                                 # build and start the GUI
```

| Option | Output |
|---|---|
| `--tokens` | token stream |
| `--ast` | abstract syntax tree |
| `--symtab` | symbol table |
| `--tac` | Three Address Code (default) |
| `--all` | all of the above |
| `-o file` | write the output to a file |
| `--emit-files` | also write `ast.txt`, `symbols.txt` and `output.tac` (the GUI uses these) |

Exit status: `0` compiled, `1` errors in the program, `2` usage or file problem. Errors are printed as `line N: <lexical|syntax|semantic> error: <description>`.

## Tests

```sh
make test                        # run every test
sh tests/run_tests.sh --update   # regenerate the .expected files after an intended change
```

Each `tests/<category>/<name>.java` has a matching `<name>.expected` file. Its first line is the exit status; the rest is the compiler's output.

| Folder | Programs |
|---|---|
| `valid/` | do_while, for_loop, java_wrapper, manual_sample, manual_tac_example, nested_scopes, smoke, sum_three, switch_case |
| `lexical/` | bad_character, invalid_identifier, unterminated_comment |
| `syntax/` | missing_parenthesis, missing_semicolon, multiple_errors |
| `semantic/` | break_outside_loop, invalid_assignment, invalid_expression, non_boolean_condition, redeclaration, scope_violation, type_mismatch, undeclared, uninitialized |
