# compiler: the C++ compiler

**Author:** Abdul Julil Shahria · part of [Tricora](../../readme.md), the Compiler Construction Lab project

`compiler` is a compiler for the lab manual's mini language written in **C++** style. It is built with **Flex**, **Bison** and **C++11**. It takes a program written exactly as in the manual, or the same program written as ordinary C++ (`#include <iostream>`, `int main()`, `cout`, `cin`, `for`, `switch`, …), and produces **Three Address Code (TAC)**, the output the manual requires. Bonus phases then optimize the TAC, translate it to x86 assembly, or run it.

```text
source file
  │
  ├─► Preprocessor              src/preprocessor/    #include lines: known headers, include-before-code rule
  ├─► Lexer (Flex)              src/lexer/lexer.l    tokens, lexical errors
  ├─► Parser (Bison)            src/parser/parser.y  Abstract Syntax Tree, syntax errors, recovery
  ├─► Semantic analysis         src/semantic/  +  src/symbol_table/    scope and type errors
  ├─► TAC generation            src/tac/             Three Address Code  ◄── required output
  │
  ├─► Optimizer (bonus)         src/optimizer/       --opt
  ├─► x86 code generator (bonus) src/codegen/        --asm
  └─► TAC interpreter (bonus)   src/interpreter/     --run
```

## What it accepts

**The manual's language (section 5).** A file with no `#include` and no `main` is compiled as the manual's language, with no flags.

**The same program as C++ (bonus).** Any other file is treated as C++:

| Feature | Example |
|---|---|
| Headers | `#include <iostream>` is required (it enables `cout`, `cin` and `endl`); 84 standard C and C++ headers are recognised, `#include` lines must come before any code, and `using namespace std;` is accepted. |
| Main function | `int main() { ... }` or `int main(void)`. `void main()` is rejected (`'main' must return int`). Only variable declarations (globals) and other functions may appear outside `main`. |
| Functions | `int add(int a, int b) { return a + b; }`, `void show(float v) { ... }`, called as `add(1, 2)` or `show(x);`. Recursion works. Each argument must fit its parameter, and `return` must match the function's type. |
| Arrays | `int a[5];`, `float b[] = {1.5, 2};`, `a[i] = 3;`, `a[i] += 2;`, `a[i]++`, `cin >> a[i];`. One dimension, `int` index; `--run` reports an index out of bounds. |
| Types | `int`, `float`, `double`, `bool`, `long`, `long long`, `short`, with `const`; several declarators per line: `int a = 1, b;` |
| Output and input | `cout << "sum = " << x << endl;`, `std::cout`, `cin >> a >> b;` |
| Loops and branches | `for`, `do-while`, `switch` / `case` / `default`, `break`, `continue`, `return` |
| Operators | `x += e`, `-=`, `*=`, `/=`, `x++`, `++x`, `x--`, `--x`, and the ternary `c ? a : b` |

The full grammar, including the precedence table and the error-recovery rules, is in [`docs/grammar.md`](docs/grammar.md).

## Requirements

`g++` (C++11 or newer), `flex`, `bison` and `make`. On Windows, use MinGW/MSYS and run the commands in Git Bash. Tested with GCC 6.3, Flex 2.5.35, Bison 2.4.2 and GNU Make 3.81.

## Build

```bash
make            # writes build/compiler.exe
make clean      # deletes build/
```

Everything `make` generates (the parser and lexer sources, object files, `compiler.exe`) goes to `build/`, which is not committed.

## Run

```bash
build/compiler.exe examples/headers_demo.cpp                # Three Address Code (the default)
build/compiler.exe --all tests/valid/manual_sample_no_header.cpp   # every phase (the manual's sample)
build/compiler.exe --manual --all examples/sample.cpp       # has an #include but no main: needs --manual
build/compiler.exe --opt --run examples/floats.cpp          # several phases in one run
echo "3 2.5" | build/compiler.exe --run program.cpp         # with input for cin
build/compiler.exe -o build/out.s program.cpp               # write x86 assembly ...
gcc build/out.s -o build/out.exe                            # ... and assemble it (MinGW)
python examples/demo_programs.py                            # build/demo.html: several programs, one tab per phase
```

| Option | Output |
|---|---|
| `--headers` | the `#include` headers, with their line, category and role |
| `--tokens` | the token stream |
| `--ast` | the Abstract Syntax Tree, indented |
| `--dot` | the Abstract Syntax Tree as a Graphviz graph (bonus): `build/compiler.exe --dot prog.cpp > ast.dot`, then `dot -Tpng ast.dot -o ast.png` |
| `--symtab` | the symbol table: name, type, scope, line |
| `--tac` | Three Address Code (the default) |
| `--opt` | optimized TAC (bonus) |
| `--asm` | 32-bit x86 assembly, AT&T syntax (bonus) |
| `--run` | runs the program with the TAC interpreter (bonus) |
| `--all` | tokens, AST, symbol table, TAC, optimized TAC, assembly and the program's output |
| `-o file` | writes the assembly to `file` |
| `--allow-no-header` | does not require `#include <iostream>` |
| `--allow-no-main` | does not require `int main()` |
| `--manual` | both of the above |
| `--strict` | always requires `#include <iostream>` and `int main()`, even for a file in the manual's style |

Several phase options can be combined; each phase is printed under its own heading, such as `=== Three Address Code ===`.

**Exit status:** `0` when the program compiled (and ran), `1` when it has errors or the file cannot be opened, `2` when `--run` stops with a runtime error.

## Output of each phase

**Headers** (`--headers`, on `examples/headers_demo.cpp`)

```text
Header              Line  Category      Role in this compiler
<iostream>          2     I/O           enables cout / endl  - std::cout / std::cin streams
<vector>            3     Containers    accepted (no effect)  - dynamic array
<cmath>             4     Math          accepted (no effect)  - sqrt, pow, sin, ...
```

**Symbol table** (`--symtab`). A `for` loop variable lives in its own inner scope.

```text
Name      Type      Scope   Line
sum       int       1       6
i         int       2       7
average   float     1       10
big       bool      1       11
```

**Three Address Code** for a program that reads two numbers and prints their product:

```cpp
int count;
float price;
cin >> count >> price;
cout << count << " items x " << price << " = " << count * price << endl;
```

```text
    read count
    read price
    out count
    outs " items x "
    out price
    outs " = "
    t1 = (float) count
    t2 = t1 * price
    out t2
    outnl
```

The int is converted to float explicitly (`(float) count`) before the multiplication. With the input `3 2.5`, `--run` prints `3 items x 2.5 = 7.5`. If `cin` cannot read a number, the variable keeps its value, as in C++.

## Error messages

Errors are printed as `<Kind> error [line N]: <description>`. Error recovery reports every error in one run, and compilation stops after the first phase that reports errors.

```text
Lexical error [line 1]: missing required header: add '#include <iostream>' at the top of the file
Lexical error [line 8]: invalid identifier '2count' (cannot start with a digit)
Lexical error [line 9]: invalid character '$'

Syntax error [line 8]: syntax error, unexpected KW_COUT, expecting ';' or ','

Semantic error [line 7]: redeclaration of 'x' in the same scope (first declared at line 6)
Semantic error [line 8]: type mismatch: cannot initialize bool 'flag' with a value of type float
Semantic error [line 9]: undeclared variable 'y'
Semantic error [line 13]: scope violation: 'inner' was declared in an inner block (line 11) and is not visible here
Semantic error [line 14]: invalid assignment: cannot assign bool to int variable 'x'
Semantic error [line 15]: invalid operands to '&&': int and int (logical operators need bool operands)
```

## Directory contents

```text
c++_compiler/
├── Makefile                       make, make test, make clean
├── README.md                      this file
├── docs/
│   └── grammar.md                 formal CFG, precedence, lexical elements, error recovery
├── src/
│   ├── main.cpp                   command-line driver: runs the phases in order
│   ├── preprocessor/headers.*     known headers and the #include rules
│   ├── lexer/lexer.l              tokens, comments, #include lines, lexical errors
│   ├── parser/parser.y            grammar, AST construction, error recovery
│   ├── ast/ast.*                  AST node classes and the --ast printer
│   ├── symbol_table/symbol_table.*   stack of scopes: name, type, scope, line, TAC name
│   ├── semantic/semantic.*        scope and type checking
│   ├── tac/tac.*                  Three Address Code generation
│   ├── optimizer/optimizer.*      bonus: constant folding and propagation, dead and unreachable code
│   ├── codegen/codegen.*, assembly.*   bonus: TAC to 32-bit x86 assembly
│   ├── interpreter/interpreter.*  bonus: runs the TAC (--run)
│   └── common/                    types.h, diagnostics.* (error messages and counts)
├── examples/
│   ├── sample.cpp                 the manual's sample program (section 5.5)
│   ├── floats.cpp                 float arithmetic, int/float mixing, shadowing
│   ├── cpp_extras.cpp             for, ++/--, compound assignment, cout
│   ├── headers_demo.cpp           standard headers, using namespace std, endl
│   └── demo_programs.py           writes build/demo.html
└── tests/                         see Tests below
```

## Arrays and functions in the TAC (bonus)

An array element is read with `t1 = a[i]` and written with `a[i] = t1`; the index counts
elements. A function is a `func f:` ... `endfunc` block: each parameter is taken with
`formal x`, and `return t1` leaves it. A call passes each argument with `param`, then calls:

```text
func square:            param 4
formal x                t2 = call square 1
t1 = x * x              print t2
return t1
endfunc
```

A `func` block is skipped when execution reaches it; its code only runs through a call.
A call saves the called function's own variables, so recursion works with `--run` and in
the `--asm` output.

## Tests

```bash
make test                          # build, then run all 51 tests
bash tests/run_tests.sh --update   # rewrite the .expected files after an intended change
```

Each `tests/<category>/<name>.cpp` is paired with `<name>.expected`, which holds the expected output of `--headers --tac --opt --run`. A test can set compiler flags on its first line (`// flags: --allow-no-header`) and give `cin` input near the top (`// stdin: 2 5`).

| Folder | Count | Programs |
|---|---|---|
| `valid/` | 25 | arrays_functions, beginner_main, cin_float_bool, cpp_extras, dot_ast, fibonacci, floats_scopes, gcd_palindrome, global_variable, harmonic_ternary, headers_iostream, indented_include, loops_switch_bool, manual_sample_no_header, nested_control, primes, sample_manual, switch_continue_logic, temperature_collatz, textbook_for_loop, textbook_hello, textbook_if_else, textbook_sum, textbook_using_hello, textbook_using_sum |
| `lexical/` | 7 | bad_identifier, bad_string, include_after_code, invalid_char, missing_header, unknown_header, unterminated_comment |
| `syntax/` | 9 | bad_main, main_without_braces, main_without_type, missing_semicolon, no_main, statement_outside_main, two_mains, unbalanced, void_main |
| `semantic/` | 10 | array_function_errors, cout_without_iostream, invalid_assignment, invalid_expression, jump_errors, redeclaration, scope_violation, ternary_error, type_mismatch, undeclared |

Current result: **51 passed, 0 failed**.
