# minijavac: the Java compiler

**Author:** Saima Suchi · part of [Tricora](../../readme.md), the Compiler Construction Lab project

`minijavac` is a compiler front-end with intermediate code generation for the lab manual's mini language. It is written in **C++17** with **Flex** and **Bison**. It takes a program written exactly as in the manual, or the same program written as Java (a `class` with `main`, `System.out.println`, `Scanner` input, …), and produces **Three Address Code (TAC)**, the output the manual requires. An optional back end (bonus) can then optimize that TAC, run it, or translate it to x86 assembly.

```text
source file
  │
  ├─► Lexer (Flex)              src/lexer/lexer.l       tokens, import lines, lexical errors
  ├─► Parser (Bison)            src/parser/parser.y     Abstract Syntax Tree, syntax errors, recovery
  ├─► Semantic analysis         src/semantic/  +  src/symbol_table/    scope, type and initialization errors
  ├─► TAC generation            src/codegen/            Three Address Code  ◄── required output
  │
  └─► Back end (bonus)          src/backend/            --opt optimized TAC · --run program output · --asm x86
```

## What it accepts

**The manual's language (section 5).** A bare list of statements, exactly like the manual's sample program, is accepted with no flags. Inside a class, the manual's spellings `bool` and `print x;` still work.

**The same program as Java (bonus):**

| Feature | Example | Notes |
|---|---|---|
| Class wrapper | `class Main { public static void main(String[] args) { ... } }` (or `public class`) | `--strict` makes the wrapper compulsory. |
| Methods | `static int add(int a, int b) { return a + b; }`, `static void show(float v) { ... }` | In the class next to `main`, or among the statements in the bare form. Recursion works. A method sees its parameters and its own variables (Java has no global variables). Each argument must fit its parameter; a non-void method must end with a `return` ("missing return statement"). Array parameters are not supported. |
| Arrays | `int[] a = new int[5];`, `int[] b = {1, 2, 3};`, `a[i] = 3;`, `a[i]++`, `a.length`, `a[i] = sc.nextInt();` | One dimension; the size in `new int[n]` must be an integer literal. `--run` reports an index out of bounds. |
| Types | `int`, `float`, `boolean` (`bool` also works) | An `int` value may be stored in a `float` variable; every other mix is an error. |
| Output | `System.out.println(x);` | `print x;` also works. |
| Loops and branches | `for`, `do-while`, `switch` / `case` / `default`, `break`, `continue` | `break` is only allowed inside a loop or `switch`, `continue` inside a loop. |
| Operators | `i++`, `i--` | |
| Imports | `import java.util.Scanner;`, `import java.util.*;`, `import static java.lang.Math.max;` | Only standard `java.*` and `javax.*` packages. An import must come before the class. `--headers` lists them. |
| Input | `Scanner sc = new Scanner(System.in);` then `int n = sc.nextInt();` | Needs `import java.util.Scanner;` (or `java.util.*`). `nextInt`, `nextFloat`, `nextDouble` (read as a `float`) and `nextBoolean`. A read must be the whole right side of `=`, and its type must match the variable. |
| Java rule | `int total; total = total + 1;` | A variable must be given a value before it is used ("might not have been initialized"). |

The full grammar of the core language (BNF, precedence table, dangling-else rule, lexical rules and error recovery) is in [`docs/language-grammar.md`](docs/language-grammar.md).

## Requirements

`g++` with C++17, `make`, `flex` and `bison` (Bison 2.4 or newer).

```sh
sudo apt install build-essential flex bison      # Ubuntu / Debian
```

## Build

```sh
make            # writes build/minijavac (build/minijavac.exe on Windows)
make clean      # deletes build/ and any files written by --emit-files
```

Everything `make` generates (`parser.tab.cpp`, `parser.tab.hpp`, `lex.yy.cpp`, the executable) goes to `build/`, which is not committed.

## Run

```sh
./build/minijavac tests/valid/manual_sample.java          # Three Address Code (the default)
./build/minijavac tests/valid/manual_bare_sample.java     # the manual's sample, no class
./build/minijavac --all examples/Example.java             # every phase
make run                                                  # the same: --all on examples/Example.java
echo "3 2.5" | ./build/minijavac --run Program.java       # with input for a Scanner
./build/minijavac examples/Example.java -o out.tac        # write the output to a file
```

| Option | Output |
|---|---|
| `--headers` | the `import` lines, with their line and package |
| `--tokens` | the token stream: line, token category, lexeme |
| `--ast` | the Abstract Syntax Tree, indented |
| `--dot` | the Abstract Syntax Tree as a Graphviz graph (bonus): `build/minijavac --dot Prog.java > ast.dot`, then `dot -Tpng ast.dot -o ast.png` |
| `--symtab` | the symbol table after semantic analysis |
| `--tac` | Three Address Code (the default) |
| `--opt` | optimized TAC (bonus) |
| `--run` | runs the program with the TAC interpreter and prints its output (bonus) |
| `--asm` | 32-bit x86 assembly, AT&T syntax (bonus) |
| `--all` | every phase above |
| `-o file` | writes the output to `file` instead of the screen |
| `--strict` | accepts only the `class ... main` form; a bare list of statements is a syntax error |
| `--manual` | accepts the manual's form without a class (the default; kept for older scripts) |
| `--emit-files` | also writes `ast.txt`, `symbols.txt` and `output.tac` |

**Exit status:** `0` when the program compiled, `1` when it has errors (or, with `--run`, stopped with a runtime error), `2` for a usage or file problem.

## Output of each phase

**Imports** (`--headers`)

```text
Import                        Line  Package               Role in this compiler
java.util.Scanner             2     java.util             accepted (no effect)  - collections, Scanner, Random
```

**Symbol table** (`--symtab`). Each block, including a `for` header, has its own scope.

```text
Name            Type     Scope  Depth  Line   TAC name
sum             int      0      0      4      sum
i               int      1      1      5      i
average         float    0      0      8      average
big             boolean  0      0      9      big
```

**Three Address Code** for a program that reads two numbers and prints their product:

```java
import java.util.Scanner;

class ReadInput {
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int count = sc.nextInt();
        float price = sc.nextFloat();
        System.out.println(count * price);
    }
}
```

```text
read count
read price
t1 = count * price
print t1
```

With the input `3 2.5`, `--run` prints `7.5`. Like Java's `Scanner`, missing or wrong input stops the program with a runtime error, for example `Runtime error: no more input (the program wanted an int)`.

## Error messages

Errors are printed as `line N: <kind> error: <description>`. Error recovery reports every error in one run. Lexical and syntax errors stop compilation before semantic analysis; semantic errors stop it before TAC.

```text
line 6: lexical error: invalid identifier (cannot start with a digit) '2count'
line 7: lexical error: invalid character '$'
line 3: lexical error: unknown package 'com.example.Tools' (only java.* and javax.* standard imports are supported)

line 5: syntax error: unexpected PRINTLN on line 6 (missing ';' at the end of line 5?)
line 7: syntax error: unexpected ';'

line 5: semantic error: redeclaration of 'x' (already declared at line 4 in this scope)
line 6: semantic error: type mismatch: cannot initialize boolean variable 'flag' with a float value
line 7: semantic error: undeclared variable 'y'
line 11: semantic error: scope violation: 'inner' (declared at line 9) is not visible outside its block
line 12: semantic error: invalid assignment: cannot assign a boolean value to int variable 'x'
line 13: semantic error: invalid expression: logical operator '&&' needs boolean operands, got int and int
line 4: semantic error: variable 'total' might not have been initialized
line 6: semantic error: type mismatch: sc.nextFloat() reads a float, but 'a' is an int (use sc.nextInt())
```

## Directory contents

```text
javacompiler/
├── Makefile                       make, make test, make run, make clean
├── README.md                      this file
├── docs/
│   ├── language-grammar.md        formal CFG, precedence, lexical rules, error recovery
│   └── project-report.md          project report
├── src/
│   ├── main.cpp                   command-line driver: runs the phases in order
│   ├── lexer/lexer.l              tokens, comments, import lines, lexical errors
│   ├── imports/imports.{h,cpp}    the import table: standard packages, --headers listing
│   ├── parser/parser.y            grammar, AST construction, error recovery
│   ├── ast/ast.{h,cpp}            AST node, construction and the --ast printer
│   ├── symbol_table/symbol_table.{h,cpp}   stack of scopes: type, line, scope, TAC name, initialized
│   ├── semantic/semantic.{h,cpp}  scope, type, initialization and Scanner checks
│   ├── codegen/tac.{h,cpp}        Three Address Code generation
│   ├── backend/backend.{h,cpp}    bonus: optimizer, TAC interpreter, x86 assembly
│   └── common/                    types.h, diagnostics.h (shared error counters)
├── examples/
│   └── Example.java               sample program in Java style
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

```sh
make test                          # build, then run all 43 tests
sh tests/run_tests.sh --update     # rewrite the .expected files after an intended change
```

Each `tests/<category>/<name>.java` is paired with `<name>.expected`. Its first line is the exit status, and the rest is exactly what the compiler prints. A test can give compiler options on its first line (`// flags: --strict`) and the program's input on its second line (`// input: 41 1.25 true`).

| Folder | Count | Programs |
|---|---|---|
| `valid/` | 22 | asm_small, bare_methods, do_while, dot_ast, for_loop, imports, java_wrapper, manual_bare_sample, manual_sample, manual_tac_example, nested_scopes, opt_constant_folding, run_arrays_methods, run_division_by_zero, run_manual_bare_sample, run_scanner, run_scanner_no_input, run_stress, scanner_tac, smoke, sum_three, switch_case |
| `lexical/` | 5 | bad_character, bad_import, invalid_identifier, scanner_method, unterminated_comment |
| `syntax/` | 5 | missing_class, missing_main, missing_parenthesis, missing_semicolon, multiple_errors |
| `semantic/` | 11 | array_method_errors, break_outside_loop, invalid_assignment, invalid_expression, non_boolean_condition, redeclaration, scanner_errors, scope_violation, type_mismatch, undeclared, uninitialized |

Current result: **43 passed, 0 failed**.
