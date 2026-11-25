# tricora: the C compiler

**Author:** Sabika Rahman · part of [Tricora](../../readme.md), the Compiler Construction Lab project

`tricora` is a compiler front-end with intermediate code generation for the lab manual's mini language. It is written in **C** with **Flex** and **Bison**. It takes a program written exactly as in the manual, or the same program written as ordinary C, and produces **Three Address Code (TAC)**, the output the manual requires. An optional back end (bonus) can then optimize that TAC, run it, or translate it to x86 assembly.

```text
source file
  │
  ├─► Lexer (Flex)              src/lexer/lexer.l          tokens, #include lines, lexical errors
  ├─► Parser (Bison)            src/parser/parser.y        Abstract Syntax Tree, syntax errors, recovery
  ├─► Semantic analysis         src/semantic/  +  src/symbol_table/    scope and type errors
  ├─► TAC generation            src/codegen/               Three Address Code  ◄── required output
  │
  └─► Back end (bonus)          src/backend/               --opt optimized TAC · --run program output · --asm x86
```

## What it accepts

**The manual's language (section 5).** No flags are needed.

```c
int x;
int y;
bool flag;
x = 10;
y = 0;
flag = true;
while (x > 0) {
    y = y + x;
    x = x - 1;
}
if (flag == true) { print y; } else { print x; }
```

**The same program as ordinary C (bonus).** Each feature on the left is accepted:

| Feature | Example | Notes |
|---|---|---|
| Headers | `#include <stdio.h>` | 13 standard headers are known. `<stdio.h>` declares `printf`/`scanf`; the others are accepted and listed by `--headers`. An unknown header or `#include "file.h"` is a lexical error. |
| Main function | `int main() { ... }`, `int main(void)` | Only declarations may stand outside `main`. |
| Return | `return 0;` | Only inside `main`, and the value must be an `int`. |
| Output | `printf("x = %d, y = %.2f\n", x, y);` | `%d %i` (int, bool), `%f %.Nf %g` (float), `%%`, and the escapes `\n \t \r \0 \\ \" \'`. Each conversion is type-checked against its argument. |
| Input | `scanf("%d %f", &a, &b);` | `%d %i` read into an `int` or `bool`, `%f %g` into a `float`. Only conversions and spaces may appear in the format. |

Comments (`//` and `/* */`) and whitespace are discarded everywhere. Files can use any extension; the compiler's own tests use `.tri` and the web editor uses `.c`.

## Requirements

`gcc`, `make`, `flex` and `bison` (Bison 2.4 or newer). MinGW/MSYS, MSYS2 and Linux all work.

```sh
sudo apt install build-essential flex bison      # Ubuntu / Debian
```

## Build

```sh
make            # writes build/tricora (build/tricora.exe on Windows)
make clean      # deletes build/
```

Everything `make` generates (`parser.tab.c`, `parser.tab.h`, `lex.yy.c`, the executable) goes to `build/`, which is not committed. In VS Code, **Terminal → Run Build Task** runs `make` (see `.vscode/tasks.json`).

## Run

```sh
./build/tricora examples/valid/manual_sample.tri              # Three Address Code (the default)
./build/tricora --all examples/valid/manual_sample.tri        # every phase, one after another
./build/tricora --run program.c                               # run it and print its output
echo "3 2.5" | ./build/tricora --run program.c                # with input for scanf
./build/tricora --asm program.c -o out.s && gcc out.s -o out  # x86 program (32-bit MinGW gcc)
```

| Option | Output |
|---|---|
| `--headers` | the `#include` headers, with their line, category and role |
| `--tokens` | the token stream: line, token type, lexeme |
| `--ast` | the Abstract Syntax Tree, indented |
| `--symtab` | the symbol table after semantic analysis |
| `--tac` | Three Address Code (the default; `--ir` also works) |
| `--opt` | optimized TAC (bonus) |
| `--run` | runs the program with the TAC interpreter and prints its output (bonus) |
| `--asm` | 32-bit x86 assembly, AT&T syntax (bonus) |
| `--all` | every phase above |
| `-o file` | writes the output to `file` instead of the screen |

**Exit status:** `0` when the program compiled, `1` when it has errors (or, with `--run`, stopped with a runtime error), `2` for a usage or file problem.

## Output of each phase

The examples below come from the manual's sample program and from programs in the web editor's Examples menu.

**Tokens** (`--tokens`)

```text
Line   Token            Lexeme
2      KEYWORD_INT      int
2      IDENTIFIER       x
2      DELIMITER        ;
```

**Abstract Syntax Tree** (`--ast`)

```text
Program
  VarDecl int x (line 2)
  ...
  While (line 10)
    Condition
      BinaryOp >
        Identifier x
        IntLiteral 0
```

**Symbol table** (`--symtab`). A variable that shadows an outer one gets a unique TAC name such as `x@3`.

```text
Name             Type   Scope  Depth  Line   TAC name
x                int    0      0      2      x
y                int    0      0      3      y
flag             bool   0      0      4      flag
```

**Three Address Code** (the default)

```text
x = 10
y = 0
flag = true
L1:
t1 = x > 0
ifFalse t1 goto L2
t2 = y + x
y = t2
t3 = x - 1
x = t3
goto L1
L2:
t4 = flag == true
ifFalse t4 goto L3
print y
goto L4
L3:
print x
L4:
```

`printf` and `scanf` become TAC instructions of their own: `outs "text"`, `out x` (a `%d` or `%g` value), `outf x 2` (`%.2f`), and `read x`.

**Optimized TAC** (`--opt`). Constant propagation and folding, removal of the branch that can never run, and removal of unused temporaries. For example, `a = 4; b = a * 2 + 1; c = b - a; if (a > 100) print 0; else print c * 10;` becomes:

```text
a = 4
b = 9
c = 5
print 50
```

## Error messages

Errors are printed as `line N: <kind> error: <description>`. Error recovery reports every error in one run. Lexical and syntax errors stop compilation before semantic analysis; semantic errors stop it before TAC.

```text
line 4: lexical error: invalid identifier (cannot start with a digit) '2count'
line 5: lexical error: invalid character '$'

line 5: syntax error: unexpected PRINT on line 6 (missing ';' at the end of line 5?)
line 7: syntax error: unexpected ';'

line 3: semantic error: redeclaration of 'x' (already declared at line 2 in this scope)
line 4: semantic error: type mismatch: cannot initialize bool variable 'flag' with a float value
line 5: semantic error: undeclared variable 'y'
line 9: semantic error: scope violation: 'inner' (declared at line 7) is not visible outside its block
line 10: semantic error: invalid assignment: cannot assign a bool value to int variable 'x'
line 11: semantic error: invalid expression: logical operator '&&' needs bool operands, got int and int
```

With `--run`, a division by zero or input that does not match the variable (for example `abc` for `%d`) stops the program with a runtime error such as `Runtime error: the input 'abc' is not an int`.

## Directory contents

```text
c-compiler/
├── Makefile                       make, make test, make clean
├── README.md                      this file
├── .vscode/                       build and run tasks for VS Code
├── docs/
│   ├── grammar.md                 formal CFG, precedence, lexical rules, error recovery
│   └── PROJECT_REPORT.md          project report
├── src/
│   ├── main.c                     command-line driver: runs the phases in order
│   ├── lexer/lexer.l              tokens, comments, #include lines, lexical errors
│   ├── parser/parser.y            grammar, AST construction, error recovery
│   ├── ast/ast.{h,c}              AST nodes, constructors, --ast printer, printf format reader
│   ├── symbol_table/symbol_table.{h,c}   stack of scopes: insert, lookup, enter and leave scope
│   ├── semantic/semantic.{h,c}    scope and type checking, printf/scanf checks
│   ├── codegen/codegen.{h,c}      Three Address Code generation
│   └── backend/backend.{h,c}      bonus: optimizer, TAC interpreter, x86 assembly
├── examples/
│   ├── valid/                     control.tri, manual_sample.tri, types.tri
│   └── invalid/                   type_error.tri, undeclared.tri
└── tests/                         see Tests below
```

The full grammar (BNF, precedence table, dangling-else rule and lexical rules) is in [`docs/grammar.md`](docs/grammar.md).

## Tests

```sh
make test                          # build, then run all 39 tests
sh tests/run_tests.sh --update     # rewrite the .expected files after an intended change
```

Each `tests/<category>/<name>.tri` is paired with `<name>.expected`. Its first line is the exit status, and the rest is exactly what the compiler prints. A test can give compiler options on its first line (`// flags: --run`) and the program's input on its second line (`// input: 41 1.25`).

| Folder | Count | Programs |
|---|---|---|
| `valid/` | 19 | arithmetic, asm_printf, asm_small, c_printf, control, headers, manual_sample, manual_tac_example, nested_scopes, opt_constant_folding, run_c_crlf_includes, run_c_hello, run_c_printf, run_division_by_zero, run_manual_sample, run_scanf, run_scanf_bad_input, run_types_and_scopes, scanf_tac |
| `lexical/` | 5 | c_bad_header, c_unterminated_string, invalid_character, invalid_identifier, unterminated_comment |
| `syntax/` | 3 | missing_parenthesis, missing_semicolon, multiple_errors |
| `semantic/` | 12 | c_main_errors, c_printf_errors, c_return_outside_main, invalid_assignment, invalid_expression, multiple_errors, non_bool_condition, redeclaration, scanf_errors, scope_violation, type_mismatch, undeclared |

Current result: **39 passed, 0 failed**.
